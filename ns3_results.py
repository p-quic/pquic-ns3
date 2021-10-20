#!/usr/bin/env python3
import argparse
import datetime
import json
import os
import sys

import yaml

indent = ''


def pp():
    global indent
    indent += '  '


def mm():
    global indent
    indent = indent[:-2]


def printi(v, *args, **kwargs):
    print('%s%s' % (indent, v), *args, **kwargs)


def gdb(plugins, binary_path, args):
    return "/pquic-ns3-dce/prepare_picoquic.sh && cd $NS3_PATH && ./waf && NS_LOG= PQUIC_DEBUG=1 PQUIC_PLUGINS=%s gdb -ex 'handle SIGUSR1 nostop noprint' --args build/myscripts/%s %s" % (','.join(plugins), binary_path, ' '.join(args.split(' ')[1:]))


parser = argparse.ArgumentParser(description='Extracts and presents results from a result file')
parser.add_argument('file', nargs='?', type=argparse.FileType('r'), default=sys.stdin)
parser.add_argument('-t', '--test', help='extracts results for the test TEST')
parser.add_argument('--cmd', action='store_true', default=False, help="don't output details, only the result command lines")
parser.add_argument('--gdb', action='store_true', default=False, help="don't output details, only the result GDB command lines")
parser.add_argument('--status', type=str, choices=('failed', 'passed', 'timedout'), default='failed', help='extracts results for tests in this status')
parser.add_argument('--stats', action='store_true', default=False, help='outputs the percentage of tests passed')
parser.add_argument('--retcode', action='store_true', default=False, help='sets the return code to 0 if all tests selected pass, otherwise -1')
parser.add_argument('--total-time', action='store_true', default=False, help='outputs the amount of time the results span')
args = parser.parse_args()

results = json.load(args.file)
script_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(script_dir, 'tests.yaml')) as f:
    tests = yaml.load(f, Loader=yaml.SafeLoader)

tests_run = 0
valid_tests_run = 0
earliest = float('+inf')
latest = float('-inf')

for test, variants in results.items():
    if args.test and test != args.test:
        continue

    if not args.stats:
        printi(test)
    for variant_type, variants_results in variants.items():
        pp()
        for variant, variant_results in variants_results.items():
            if not args.stats:
                printi('%s:%s' % (variant_type, variant))
            pp()
            for r in variant_results:
                tests_run += 1
                earliest = min(r['start'], earliest)
                latest = max(r['end'], latest); status = 'passed'
                if 'Timeout reached' in r['failures']:
                    status = 'timedout'
                elif r['failures']:
                    status = 'failed'
                else:
                    valid_tests_run += 1

                if args.stats or (args.status and status != args.status):
                    continue

                if args.cmd or not args.gdb:
                    printi("%s/%s" % (os.path.dirname(tests['binaries'][test]), r['cmdline']))
                if args.gdb:
                    printi(gdb(tests['plugins'][variant]['plugins'], tests['binaries'][test], r['cmdline']))

                if args.cmd or args.gdb:
                    continue

                pp()
                printi('Duration:', '%.02fs' % (r['end'] - r['start']))
                printi('Transfer time:', r['transfer_time'])
                printi('Status:', status)
                if status == 'failed':
                    printi('Failures:')
                    pp()
                    for f in r['failures']:
                        printi(f)
                    mm()
                mm()
            mm()

        mm()

if args.stats and tests_run:
    print('%d/%d (%.2f%%) tests passed' % (valid_tests_run, tests_run, (valid_tests_run / tests_run) * 100))

if args.total_time:
    print('Tests started at %s and stopped at %s, running for %s' % (datetime.datetime.fromtimestamp(earliest).strftime('%c'), datetime.datetime.fromtimestamp(latest).strftime('%c'), str(datetime.timedelta(seconds=latest - earliest))))

if args.retcode and valid_tests_run < tests_run:
    exit(-1)
