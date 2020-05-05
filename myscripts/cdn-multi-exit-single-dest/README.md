# Multipath Point-to-Point Link

This scenario has the following configurable properties:

* `--delay`: One-way delay of network. Specify with units. This is a required
  parameter. For example `--delay=15ms`.

* `--bandwidth`: Bandwidth of the link. Specify with units. This is a required
  parameter. For example `--bandwidth=10Mbps`. Specifying a value larger than
  10Mbps may cause the simulator to saturate the CPU.

* `--queue`: Queue size of the queue attached to the link. Specified in
  packets. This is a required parameter. For example `--queue=25`.

* `--paths`: Number of paths to create. This is a required parameter

For example,
```bash
./run.sh "multipath-p2p --delay=15ms --bandwidth=10Mbps --queue=25 --paths=2"
```
