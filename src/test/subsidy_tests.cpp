// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2025 The 405Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <validation.h>

#include <test/test_405Coin.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(subsidy_tests, TestingSetup
)

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    const auto &consensus = chainParams->GetConsensus();

    const uint32_t nPrevBits = 0x1c0fffff;
    CAmount nSubsidy = GetBlockSubsidy(nPrevBits, 0, consensus, false);
    CAmount expectedPremine = consensus.nPremineSubsidy > 0 ? consensus.nPremineSubsidy : consensus.nInitialSubsidy;
    BOOST_CHECK_EQUAL(nSubsidy, expectedPremine);

    const int32_t firstNormalBlockPrevHeight = 1;
    nSubsidy = GetBlockSubsidy(nPrevBits, firstNormalBlockPrevHeight, consensus, false);
    BOOST_CHECK_EQUAL(nSubsidy, consensus.nInitialSubsidy);

    const int32_t firstHalvingPrevHeight = consensus.nSubsidyHalvingInterval - 1;
    nSubsidy = GetBlockSubsidy(nPrevBits, firstHalvingPrevHeight, consensus, false);
    BOOST_CHECK_EQUAL(nSubsidy, consensus.nInitialSubsidy >> 1);

    const int32_t secondHalvingPrevHeight = 2 * consensus.nSubsidyHalvingInterval - 1;
    nSubsidy = GetBlockSubsidy(nPrevBits, secondHalvingPrevHeight, consensus, false);
    BOOST_CHECK_EQUAL(nSubsidy, consensus.nInitialSubsidy >> 2);

    const int32_t exhaustedHeight = 64 * consensus.nSubsidyHalvingInterval;
    nSubsidy = GetBlockSubsidy(nPrevBits, exhaustedHeight, consensus, false);
    BOOST_CHECK_EQUAL(nSubsidy, 0);
}

BOOST_AUTO_TEST_SUITE_END()
