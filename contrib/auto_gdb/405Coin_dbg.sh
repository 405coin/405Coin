#!/bin/bash
# use testnet settings,  if you need mainnet,  use ~/.405Coincore/405Coind.pid file instead
405Coin_pid=$(<~/.405Coincore/testnet3/405Coind.pid)
sudo gdb -batch -ex "source debug.gdb" 405Coind ${405Coin_pid}
