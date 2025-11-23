405Coin Core
============

> "Error 405: Profit Not Found - but the memes were priceless."

405Coin is a Ghostrider fork with CPU mining, 2-minute blocks, and a Smartnode network.

Overview
--------

Blocks land roughly every two minutes, secured by Smartnodes. Rewards start at 1,927,405 and reduce about every four years, keeping a playful CPU-mining spirit and simple network setup.

Key Specs
---------

| Spec | Value |
| --- | --- |
| Ticker | 405 |
| Algorithm | Ghostrider |
| Ports | P2P 10226 / RPC 10225 |
| Block Time | ~120 s |
| Initial Block Reward | 1,927,405 |
| Halving | ~every 4 years |
| Premine | 20,000,000,405 |
| Max Supply | ~4.05B 405 coins |

Smartnode Collateral Schedule
-----------------------------

Smartnodes activate at block 5,761 with increasing collateral that keeps the network resilient and rewards long-term participants.

- 110,000 - blocks 5,761-88,720
- 150,000 - blocks 88,721-132,720
- 190,000 - blocks 132,721-176,720
- 240,000 - blocks 176,721-220,720
- 280,000 - blocks 220,721-264,720
- 350,000 - from block 264,721

Community and Socials
---------------------

- GitHub: https://github.com/405coin/405Coin/
- Twitter: https://x.com/405Coin
- Discord: https://discord.gg/dCquzjap

License
-------

405Coin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is meant to be stable. Development is done in separate branches.
[Tags](https://github.com/405coin/405Coin/tags) are created to indicate new official,
stable release versions of 405Coin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.
