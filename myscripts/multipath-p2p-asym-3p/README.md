# Multipath Point-to-Point Link

This scenario has the following configurable properties:

* `--delay`: One-way delay of network. Specify with units. This is a required
  parameter. For example `--delay=15ms`.

* `--bandwidth`: Bandwidth of the link. Specify with units. This is a required
  parameter. For example `--bandwidth=10Mbps`. Specifying a value larger than
  10Mbps may cause the simulator to saturate the CPU.

For example,
```bash
./run.sh "multipath-p2p-asym --delay=10ms --bandwidth=20Mbps --filesize=1000000 --delay_balance=0.25 --bandwidth_balance=0.18"
```
