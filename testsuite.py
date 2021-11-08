#!/usr/bin/env python3
import builtins
import json
import multiprocessing
import os
import shutil
import subprocess
import sys
import argparse
import tempfile
from functools import partial
from time import sleep, time

import yaml

from experimental_design import load_wsp, ParamsGenerator

if not os.path.exists('/.dockerenv'):
    print('This script is meant to be run inside the container', file=sys.stderr)
    exit(-1)

parser = argparse.ArgumentParser(description='Runs a suite of NS3 DCE tests against PQUIC')
parser.add_argument('-t', '--test', type=str, default=None, help='Only runs TEST')
parser.add_argument('-r', '--results', type=str, default='results.json', metavar='FILE', help='Stores the results in FILE (default results.json)')
parser.add_argument('-d', '--debug', action='store_true', help='Turns on debugging')
parser.add_argument('-q', '--with-qlog', action='store_true', help='Produces QLOG files when running experiments')
parser.add_argument('-m', '--multiple-pns', action='store_true', help='Explore multiple packet number spaces')
parser.add_argument('-p', '--process-qlog', action='store_true', help='Do not record full QLOG, but their processed form')
parser.add_argument('-c', '--congestion-control', type=str, default='cubic', help='Use the provided congestion control (default cubic)')
test_args = parser.parse_args()

script_dir = os.path.dirname(os.path.abspath(__file__))
ns3_dir = os.environ['NS3_PATH']
pquic_dir = os.environ['DCE_PATH']

print = partial(print, flush=True)


def run(*args, stdout=True, stderr=True, shell=True, env=None, timeout=None):
    kwargs = {}
    if not stdout:
        kwargs['stdout'] = subprocess.DEVNULL
    if not stderr:
        kwargs['stderr'] = subprocess.DEVNULL
    if env:
        kwargs['env'] = os.environ.copy()
        kwargs['env'].update(env)

    p = subprocess.Popen(args, universal_newlines=True, shell=shell, **kwargs)
    try:
        return p.wait(timeout=timeout)
    except:
        p.terminate()
        return 'timeout'


def build_ns3():
    os.chdir(ns3_dir)
    if run('./waf', stderr=test_args.debug) is not 0:
        print('Building ns-3 failed', file=sys.stderr)
        exit(0)
    os.chdir(script_dir)


def build_picoquic():
    if run(os.path.join(script_dir, 'prepare_picoquic.sh'), stdout=test_args.debug, stderr=test_args.debug) is not 0:
        print('Building pquic failed', file=sys.stderr)
        exit(0)


def compute_queue(bandwidth, delay):
    return 1.5 * (bandwidth / 8) * 1024 * 1024 * (2 * delay / 1000) // 1200


def read_all(filename, mode='r'):
    with open(filename, mode=mode) as f:
        return f.read()


def run_binary(tests, binary, params, values, sim_timeout, hard_timeout, env=None):
    core = os.getpid() % os.cpu_count()
    os.sched_setaffinity(os.getpid(), {core})

    bw, fs = values['bandwidth'][0], values['filesize'][0]
    failures = []

    params['queue'] = {'type': int}
    values['queue'] = [compute_queue(values['bandwidth'][0], values['delay'][0])]
    args = []
    for p, v in values.items():
        value_str = ','.join(('%d' if params[p]['type'] is int else '%.2f') % e for e in v)
        args.append('--%s=%s%s' % (p, value_str, params[p].get('units', '')))

    with tempfile.TemporaryDirectory(prefix='pquic_ns3_%s_' % b) as tmp_dir:
        os.chdir(tmp_dir)
        shutil.copytree(os.path.join(ns3_dir, 'files-0'), os.path.join(tmp_dir, 'files-0'), symlinks=True)
        shutil.copytree(os.path.join(ns3_dir, 'files-1'), os.path.join(tmp_dir, 'files-1'), symlinks=True)

        start = time()
        if (fs / 1024 / 1024) / (bw / 8) > sim_timeout:
            print("Transfering %d bytes over a %.2fMbps link in less than %d secs is deemed impossible, skipping" % (fs, bw, sim_timeout), file=sys.stderr)
            failures.append("Link speed and filesize do not match timeout")
        else:
            print('Test started:', binary, ' '.join(args), env)
            c = run(os.path.join(ns3_dir, 'build', 'myscripts', tests['binaries'][binary]), *args, shell=False, env=env, timeout=hard_timeout)
            if c is not 0 and c != 'timeout':
                print("Failed test: %s returned %d" % (b, c), file=sys.stderr)
                failures.append("Failed test")
            elif c == 'timeout':
                print("Timeout reached", file=sys.stderr)
                failures.append("Timeout reached")

        end = time()

        client_stdout, server_stdout = None, None
        client_stderr, server_stderr = None, None
        client_status, server_status = None, None
        client_qlog, server_qlog = None, None

        for root, dirs, files in os.walk(tmp_dir):
            if 'stdout' in files:
                if 'files-0' in root:
                    client_stdout = read_all(os.path.join(root, 'stdout'))
                elif 'files-1' in root:
                    server_stdout = read_all(os.path.join(root, 'stdout'))
            if 'stderr' in files:
                if 'files-0' in root:
                    client_stderr = read_all(os.path.join(root, 'stderr'))
                elif 'files-1' in root:
                    server_stderr = read_all(os.path.join(root, 'stderr'))
            if 'status' in files:
                if 'files-0' in root:
                    client_status = read_all(os.path.join(root, 'status'))
                elif 'files-1' in root:
                    server_status = read_all(os.path.join(root, 'status'))

            for qlog_file in [f for f in files if '.qlog' in f]:
                if '.client.qlog' in qlog_file and 'files-0' in root:
                    full_path = os.path.join(root, qlog_file)
                    # Because QLOG might be ill formatted.
                    run('sed -i "s;,]}]};]}]};g" ' + str(full_path))
                    if test_args.process_qlog:
                        cmd = "{}/frame-extractor -q -a -b {} out.qlog".format(script_dir, full_path)
                        run(cmd)
                        client_qlog = json.loads(read_all('out.qlog'))
                    else:
                        client_qlog = json.loads(read_all(full_path))

                elif '.server.qlog' in qlog_file and 'files-1' in root:
                    full_path = os.path.join(root, qlog_file)
                    # Because QLOG might be ill formatted.
                    run('sed -i "s;,]}]};]}]};g" ' + str(full_path))
                    if test_args.ack_ranges:
                        cmd = "{}/frame-extractor -q -s -b {} out.qlog".format(script_dir, full_path)
                        run(cmd)
                        server_qlog = json.loads(read_all('out.qlog'))
                    else:
                        server_qlog = json.loads(read_all(full_path))

        # Check that both are disconnected
        if server_stdout is not None  and 'No more active connections.' not in server_stdout:
            failures.append('Server not disconnected')
        if client_stdout is not None  and 'All done, Closing the connection.' not in client_stdout and 'Received a request to close the connection.' not in client_stdout:
            failures.append('Client not disconnected')

        # Check that the file was successfully transferred
        if client_stdout is not None  and '-1.0' in client_stdout:
            failures.append('Client did not receive the file')

        # Check the client return code
        if client_stdout is not None and 'Client exit with code = 0\n' not in client_stdout:
            failures.append('Client exit code was not 0')

        transfer_time = None
        if failures:
            print(repr(client_status), repr(client_stdout[:10000]))
            print('-' * 20)
            print(repr(server_status), repr(server_stdout[:10000]))
            print('-' * 20)
            print(repr(client_stderr[:10000]), repr(client_stderr[:10000]))
            print(failures)
            print('Test crashed:', binary, ' '.join(args), env, 'after (real-time) %.2fs' % (end - start))
        else:
            transfer_time = ' '.join(client_stdout.splitlines()[-12].split()[4:6])
            print('Test finished:', binary, ' '.join(args), env, 'in (simulated)', transfer_time, '(real-time) %.2fs' % (end - start))

        return {'start': start, 'end': end, 'values': values, 'cmdline': '%s %s' % (binary, ' '.join(args)), 'failures': failures, 'transfer_time': transfer_time, 'client_qlog': client_qlog, 'server_qlog': server_qlog}


results = {}

wsp_matrix = load_wsp(os.path.join(script_dir, 'wsp_20_col'), 20, 95)
with open(os.path.join(script_dir, 'tests.yaml')) as f:
    tests = yaml.load(f)

build_ns3()
build_picoquic()
os.chdir(ns3_dir)
for b, opts in tests['definitions'].items():
    results = {}

    if test_args.test and b != test_args.test:
        continue
    if b not in tests['binaries']:
        print('Unknown binary: %s' % b, file=sys.stderr)
        continue

    params = opts['params']
    for p, attrs in params.items():
        if attrs['type'] in vars(builtins):
            attrs['type'] = vars(builtins)[attrs['type']]

    for p_id in opts['variants']['plugins']:
        for f in opts['variants'].get('filesize', [None]):
            if f and not 'filesize' in params:
                params['filesize'] = {'range': [f, f], 'type': type(f)}

            with multiprocessing.Pool(processes=os.environ.get('NPROC')) as pool:
                r = results.get(p_id, [])
                env = {"PICOQUIC_CC": test_args.congestion_control}
                if test_args.with_qlog:
                    env["PICOQUIC_QLOG"] = "1"
                if test_args.multiple_pns:
                    env["PICOQUIC_MULTIPLE_PNS"] = "1"
                r.extend(pool.starmap(run_binary, [(tests, b, params, v, opts['sim_timeout'], opts['hard_timeout'], env) for v in ParamsGenerator(params, wsp_matrix).generate_all_values()]))
                results[p_id] = r

            if f:
                del params['filesize']

    result_fname = os.path.join(os.path.dirname(test_args.results), "{}-{}".format(b, os.path.basename(test_args.results)))
    with open(result_fname, 'w') as f:
        json.dump(results, f)