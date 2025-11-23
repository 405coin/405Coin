// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2020 The Dash Core developers
// Copyright (c) 2025 The 405Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <llmq/quorums_parameters.h>
#include <tinyformat.h>
#include <update/update.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <arith_uint256.h>

#include <assert.h>
#include <iostream>
#include <string>
#include <memory>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <chainparamsseeds.h>

static size_t lastCheckMnCount = 0;
static int lastCheckHeight = 0;
static bool lastCheckedLowLLMQParams = false;

static std::unique_ptr <CChainParams> globalChainParams;

static CBlock
CreateGenesisBlock(const char *pszTimestamp, const CScript &genesisOutputScript, uint32_t nTime, uint32_t nNonce,
                   uint32_t nBits, int32_t nVersion, const CAmount &genesisReward) {
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4)
                                       << std::vector<unsigned char>((const unsigned char *) pszTimestamp,
                                                                     (const unsigned char *) pszTimestamp +
                                                                     strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

static CBlock
CreateDevNetGenesisBlock(const uint256 &prevBlockHash, const std::string &devNetName, uint32_t nTime, uint32_t nNonce,
                         uint32_t nBits, const CAmount &genesisReward) {
    assert(!devNetName.empty());

    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    // put height (BIP34) and devnet name into coinbase
    txNew.vin[0].scriptSig = CScript() << 1 << std::vector<unsigned char>(devNetName.begin(), devNetName.end());
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = CScript() << OP_RETURN;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = 4;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock = prevBlockHash;
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock
CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount &genesisReward) {
    const char *pszTimestamp = "Error 405: Profit Not Found - but the memes were priceless";
    const CScript genesisOutputScript = CScript() << ParseHex(
            "04a0ce42f7d39022ab7a2c1d8418f2ad1960f70f9137bac9d0f74d2a3c0b15c567a15d096b4fd1f8b98a90b6a0c7ad719f8d92be7c3c00759222af1abcf4f5976dd9da2bf0fd6a94ae8eac893667d4480b41eaf9c572cca5f8d7b3cc3dba938c")
                                                  << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

static CBlock FindDevNetGenesisBlock(const CBlock &prevBlock, const CAmount &reward) {
    std::string devNetName = gArgs.GetDevNetName();
    assert(!devNetName.empty());

    CBlock block = CreateDevNetGenesisBlock(prevBlock.GetHash(), devNetName.c_str(), prevBlock.nTime + 1, 0,
                                            prevBlock.nBits, reward);

    arith_uint256 bnTarget;
    bnTarget.SetCompact(block.nBits);

    for (uint32_t nNonce = 0; nNonce < UINT32_MAX; nNonce++) {
        block.nNonce = nNonce;

        uint256 hash = block.GetHash();
        if (UintToArith256(hash) <= bnTarget)
            return block;
    }

    // This is very unlikely to happen as we start the devnet with a very low difficulty. In many cases even the first
    // iteration of the above loop will give a result already
    error("FindDevNetGenesisBlock: could not find devnet genesis block for %s", devNetName);
    assert(false);
}

/// Verify the POW hash is valid for the genesis block
/// If starting Nonce is not valid, search for one
static void VerifyGenesisPOW(const CBlock &genesis) {
    arith_uint256 bnTarget;
    bnTarget.SetCompact(genesis.nBits);

    CBlock block(genesis);
    do {
        uint256 hash = block.GetPOWHash();
        if (UintToArith256(hash) <= bnTarget) {
            if (genesis.nNonce != block.nNonce) {
                std::cerr << "VerifyGenesisPOW:  provided nNonce (" << genesis.nNonce << ") invalid" << std::endl;
                std::cerr << "   nonce: " << block.nNonce << ", pow hash: 0x" << hash.ToString()
                          << ", block hash: 0x" << block.GetHash().ToString() << std::endl;
                assert(genesis.nNonce == block.nNonce);
            } else {
                return;
            }
        }
        ++block.nNonce;
    } while (block.nNonce != 0);

    // We should never get here
    error("VerifyGenesisPOW: could not find valid Nonce for genesis block");
    assert(false);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */


class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.nSubsidyHalvingInterval = 1051200; // ~4 years at 2-minute blocks
        consensus.nInitialSubsidy = 1927 * COIN;
        consensus.nPremineSubsidy = 20000000 * COIN;
        consensus.nPremineHeight = 1;
        consensus.nSmartnodePaymentsStartBlock = 5761; //
        consensus.nSmartnodePaymentsIncreaseBlock = 158000; // actual historical value
        consensus.nSmartnodePaymentsIncreasePeriod = 576 * 30; // 17280 - actual historical value
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = INT_MAX; // actual historical value
        consensus.nBudgetPaymentsCycleBlocks = 16616; // ~(60*24*30)/2.6, actual number of blocks per month is 200700 / 12 = 16725
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = INT_MAX; // The block at which 12.1 goes live (end of final 12.0 budget cycle)
        consensus.nSuperblockStartHash = uint256S("0000000000020cb27c7ef164d21003d5d20cdca2f54dd9a9ca6d45f4d47f8aa3");
        consensus.nSuperblockCycle = 16616; // ~(60*24*30)/2.6, actual number of blocks per month is 200700 / 12 = 16725
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nSmartnodeMinimumConfirmations = 15;
        consensus.BIPCSVEnabled = true;
        consensus.BIP147Enabled = true;
        consensus.BIP34Enabled = true;
        consensus.BIP65Enabled = true; // 00000000000076d8fcea02ec0963de4abfd01e771fec0863f960c2c64fe6f357
        consensus.BIP66Enabled = true; // 00000000000b1fa2dfa312863570e13fae9ca7b5566cb27e55422620b469aefa
        consensus.DIP0001Enabled = true;
        consensus.DIP0003Enabled = true;
        consensus.DIP0008Enabled = true;
        // consensus.DIP0003EnforcementHeight = 1047200;
        consensus.powLimit = uint256S(
                "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 24 * 60 * 60; // 405Coin: 1 day
        consensus.nPowTargetSpacing = 2 * 60; // 405Coin: 2 minutes
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPowDGWHeight = 60;
        consensus.DGWBlocksAvg = 60;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.smartnodePaymentFixedBlock = 6800;
        consensus.nFutureForkBlock = 420420;

        updateManager.Add
                (   // V17 voting blocks 419328-427391 in mainnet, 4032 voting, 4032 grace period, active at 427392
                        Update(EUpdate::DEPLOYMENT_V17, std::string("v17"), 0, 4032, 419328, 1, 3, 1, false,
                               VoteThreshold(80, 60, 5), VoteThreshold(0, 0, 1), false, 427392)
                );
        updateManager.Add(
                Update(EUpdate::ROUND_VOTING, std::string("Round Voting"),
                       1,                        // bit
                       720,                     // roundSize
                       905760,                    // startHeight
                       7,                        // votingPeriod
                       365,                      // votingMaxRounds
                       7,                        // gracePeriod
                       false,                    // forceUpdate
                       VoteThreshold(85, 85, 1), // minerThreshold
                       VoteThreshold(0, 0, 1))   // nodeThreshold
        );

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x34; // '4'
        pchMessageStart[1] = 0x30; // '0'
        pchMessageStart[2] = 0x35; // '5'
        pchMessageStart[3] = 0x4d; // 'M'
        nDefaultPort = 10226;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 7;
        m_assumed_chain_state_size = 2;
        //FindMainNetGenesisBlock(1762628736, 0x20001fff, "main");
        genesis = CreateGenesisBlock(1762628736, 7958, 0x20001fff, 4, 5000 * COIN);
        VerifyGenesisPOW(genesis);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0xcdd77255d68edd62e7e41ba53f044976a57f396fdb1b4b4486f304d27e8d57d0"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x7a4b1fc5aa80ec3f2bf6edb718b713230186c13ff16363dc4a554485e9465023"));

        vSeeds.emplace_back("mn01.405.mn");
        vSeeds.emplace_back("mn02.405.mn");

        // 405Coin addresses start with '4o5'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>{0x7B, 0xEA};
        // 405Coin script addresses start with '7'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 16);
        // 405Coin private keys start with '7' or 'X'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 128);
        // 405Coin BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        // 405Coin BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        // 405Coin BIP44 coin type is '5'
        std::string strExtCoinType = gArgs.GetArg("-extcoinindex", "");
        nExtCoinType = strExtCoinType.empty() ? 200 : std::stoi(strExtCoinType);
//        if(gArgs.GetChainName() == CBaseChainParams::MAIN) {
//        	std::cout << "mainnet is disable" << endl;
//        	exit(0);
//        }
        std::vector <FounderRewardStructure> rewardStructures = {{INT_MAX, 5}};// 5% founder/dev fee forever
        consensus.nFounderPayment = FounderPayment(rewardStructures, 250);
        consensus.nCollaterals = SmartnodeCollaterals(
                {{88720,   110000 * COIN},
                 {132720,  150000 * COIN},
                 {176720,  190000 * COIN},
                 {220720,  240000 * COIN},
                 {264720,  280000 * COIN},
                 {INT_MAX, 350000 * COIN}},
                {{5761,    0},
                 {INT_MAX, 20}}
        );
        //FutureRewardShare defaultShare(0.8,0.2,0.0);
        consensus.nFutureRewardShare = Consensus::FutureRewardShare(0.8, 0.2, 0.0);

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq3_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq20_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq20_85;
        consensus.llmqs[Consensus::LLMQ_100_67] = Consensus::llmq100_67_mainnet;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_400_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_50_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_100_67;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;
        miningRequiresPeers = true;
        nLLMQConnectionRetryTimeout = 60;
        m_is_mockable_chain = false;

        nPoolMinParticipants = 3;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour

        vSporkAddresses = {"4o5WadAnGCypB61cd97LJqGpuPsME85KDL2s"};
        nMinSporkKeys = 1;
        fBIP9CheckSmartnodesUpgraded = true;

        checkpointData = {};

        chainTxData = ChainTxData{
                static_cast<int64_t>(genesis.nTime),
                0,
                0.0
        };
    }
};

/**
 * Testnet (v4)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nInitialSubsidy = 1000 * COIN;
        consensus.nPremineSubsidy = 0;
        consensus.nPremineHeight = 0;
        consensus.nSmartnodePaymentsStartBlock = 1000; // not true, but it's ok as long as it's less then nSmartnodePaymentsIncreaseBlock
        consensus.nSmartnodePaymentsIncreaseBlock = 4030;
        consensus.nSmartnodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = INT_MAX;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = INT_MAX; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
        consensus.nSuperblockStartHash = uint256(); // do not check this on testnet
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nSmartnodeMinimumConfirmations = 1;
        consensus.BIP34Enabled = true;
        consensus.BIP65Enabled = true; // 0000039cf01242c7f921dcb4806a5994bc003b48c1973ae0c89b67809c2bb2ab
        consensus.BIP66Enabled = true; // 0000002acdd29a14583540cb72e1c5cc83783560e38fa7081495d474fe1671f7
        consensus.DIP0001Enabled = true;
        consensus.DIP0003Enabled = true;
        consensus.BIPCSVEnabled = true;
        consensus.BIP147Enabled = true;
        consensus.DIP0008Enabled = true;
        // consensus.DIP0003EnforcementHeight = 7300;
        consensus.powLimit = uint256S(
                "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 24 * 60 * 60; // 405Coin: 1 day
        consensus.nPowTargetSpacing = 60; // 405Coin: 1 minutes
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nPowDGWHeight = 60;
        consensus.DGWBlocksAvg = 60;
        consensus.smartnodePaymentFixedBlock = 1;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.nFutureForkBlock = 1000;

        updateManager.Add(
            Update(EUpdate::DEPLOYMENT_V17, std::string("v17"), 0, 1440, 25920, 7, 365, 7, false,
                VoteThreshold(95, 85, 5), VoteThreshold(0, 0, 1)));
        updateManager.Add(
            Update(EUpdate::ROUND_VOTING, std::string("Round Voting"),
                1,                        // bit
                1440,                     // roundSize
                27360,                    // startHeight
                7,                        // votingPeriod
                365,                      // votingMaxRounds
                7,                        // gracePeriod
                false,                    // forceUpdate
                VoteThreshold(85, 85, 1), // minerThreshold
                VoteThreshold(0, 0, 1))   // nodeThreshold
        );
        updateManager.Add(
            Update(EUpdate::QUORUMS_200_8, std::string("Quorums 200/8"),
                2,                        // bit
                1440,                     // roundSize
                79200,                    // startHeight
                3,                        // votingPeriod
                365,                      // votingMaxRounds
                2,                        // gracePeriod
                false,                    // forceUpdate
                VoteThreshold(85, 85, 1), // minerThreshold
                VoteThreshold(85, 85, 1)) // nodeThreshold
        );

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0"); // 0

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0"); // 0

        pchMessageStart[0] = 0x34; // '4'
        pchMessageStart[1] = 0x30; // '0'
        pchMessageStart[2] = 0x35; // '5'
        pchMessageStart[3] = 0x54; // 'T'
        nDefaultPort = 10230;
        nPruneAfterHeight = 1000;
        genesis = CreateGenesisBlock(1762629036, 1713, 0x20001fff, 4, 5000 * COIN);
        VerifyGenesisPOW(genesis);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0xf814306e55fbf69d7435940bdef6c3fc5a4838d32444524fbf3d079b0d65449d"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x7a4b1fc5aa80ec3f2bf6edb718b713230186c13ff16363dc4a554485e9465023"));

        vFixedSeeds.clear();
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("47.151.9.131");
        vSeeds.emplace_back("lbtn.405Coin.com");

        // Testnet 405Coin addresses start with 'r'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 123);
        // Testnet 405Coin script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
        // Testnet 405Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        // Testnet 405Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // Testnet 405Coin BIP44 coin type is '1' (All coin's testnet default)
        std::string strExtCoinType = gArgs.GetArg("-extcoinindex", "");
        nExtCoinType = strExtCoinType.empty() ? 10227 : std::stoi(strExtCoinType);


        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq3_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq20_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq20_85;
        consensus.llmqs[Consensus::LLMQ_100_67] = Consensus::llmq100_67_testnet;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_400_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_50_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_100_67;

        consensus.nCollaterals = SmartnodeCollaterals(
                {{INT_MAX, 60000 * COIN}},
                {{INT_MAX, 20}});

        consensus.nFutureRewardShare = Consensus::FutureRewardShare(0.8, 0.2, 0.0);

        std::vector <FounderRewardStructure> rewardStructures = {{INT_MAX, 5}};// 5% founder/dev fee forever
        consensus.nFounderPayment = FounderPayment(rewardStructures, 100, "rghjACzPtVAN2wydgDbn9Jq1agREu6rH1e");

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 60;
        // miningRequiresPeers = true;
        m_is_mockable_chain = false;
        miningRequiresPeers = false;

        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"rsqc2caFRG6myRdzKipP4PpVW9LnFaG7CH"};
        nMinSporkKeys = 1;
        fBIP9CheckSmartnodesUpgraded = true;

        checkpointData = {};

        chainTxData = ChainTxData{
                static_cast<int64_t>(genesis.nTime),
                0,
                0.0
        };

    }
};

/**
 * Devnet
 */
class CDevNetParams : public CChainParams {
public:
    explicit CDevNetParams(const ArgsManager &args) {
        strNetworkID = CBaseChainParams::DEVNET;
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nInitialSubsidy = 1000 * COIN;
        consensus.nPremineSubsidy = 0;
        consensus.nPremineHeight = 0;
        consensus.nSmartnodePaymentsStartBlock = 4010; // not true, but it's ok as long as it's less then nSmartnodePaymentsIncreaseBlock
        consensus.nSmartnodePaymentsIncreaseBlock = 4030;
        consensus.nSmartnodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4100;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = 4200; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
        consensus.nSuperblockStartHash = uint256(); // do not check this on devnet
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on devnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nSmartnodeMinimumConfirmations = 1;
        consensus.BIP147Enabled = true;
        consensus.BIPCSVEnabled = true;
        consensus.BIP34Enabled = true; // BIP34 activated immediately on devnet
        consensus.BIP65Enabled = true; // BIP65 activated immediately on devnet
        consensus.BIP66Enabled = true; // BIP66 activated immediately on devnet
        consensus.DIP0001Enabled = true; // DIP0001 activated immediately on devnet
        consensus.DIP0003Enabled = true; // DIP0003 activated immediately on devnet
        consensus.DIP0008Enabled = true;// DIP0008 activated immediately on devnet
        // consensus.DIP0003EnforcementHeight = 2; // DIP0003 activated immediately on devnet
        consensus.powLimit = uint256S(
                "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPowTargetTimespan = 24 * 60 * 60; // 405Coin: 1 day
        consensus.nPowTargetSpacing = 2 * 60; // 405Coin: 2 minutes
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPowDGWHeight = 60;
        consensus.DGWBlocksAvg = 60;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.nFutureForkBlock = 1;

        updateManager.Add
                (
                        Update(EUpdate::DEPLOYMENT_V17, std::string("v17"), 0, 10, 0, 10, 100, 10, false,
                               VoteThreshold(95, 95, 5), VoteThreshold(0, 0, 1))
                );
        updateManager.Add
                (
                        Update(EUpdate::ROUND_VOTING, std::string("Round Voting"), 1, 10, 100, 5, 10, 5, false,
                               VoteThreshold(85, 85, 1), VoteThreshold(0, 0, 1))
                );

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        pchMessageStart[0] = 0x34; // '4'
        pchMessageStart[1] = 0x30; // '0'
        pchMessageStart[2] = 0x35; // '5'
        pchMessageStart[3] = 0x44; // 'D'
        nDefaultPort = 19799;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateDevnetSubsidyAndDiffParametersFromArgs(args);
        genesis = CreateGenesisBlock(1762629336, 2841, 0x20001fff, 4, 5000 * COIN);
        VerifyGenesisPOW(genesis);
        consensus.hashGenesisBlock = genesis.GetHash();
//      std::cout << "hash: " << consensus.hashGenesisBlock.ToString() << std::endl;
        assert(consensus.hashGenesisBlock ==
               uint256S("0x6d195b5563f3fea862d1ad151f3de7e568bb139ea3d8ee68b398580e493a504c"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x7a4b1fc5aa80ec3f2bf6edb718b713230186c13ff16363dc4a554485e9465023"));
        devnetGenesis = FindDevNetGenesisBlock(genesis, 5000 * COIN);
        consensus.hashDevnetGenesisBlock = devnetGenesis.GetHash();

        consensus.nFutureRewardShare = Consensus::FutureRewardShare(0.8, 0.2, 0.0);

        std::vector <FounderRewardStructure> rewardStructures = {{INT_MAX, 5}};// 5% founder/dev fee forever
        consensus.nFounderPayment = FounderPayment(rewardStructures, 200, "yYhBxduZLMnancMkpzvcLFCiTgZRSk8wun");
        consensus.nCollaterals = SmartnodeCollaterals(
                {{INT_MAX, 60000 * COIN}},
                {{INT_MAX, 20}});

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("47.151.26.43");
        //vSeeds.push_back(CDNSSeedData("405Coinevo.org",  "devnet-seed.405Coinevo.org"));

        // Testnet 405Coin addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
        // Testnet 405Coin script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
        // Testnet 405Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        // Testnet 405Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // Testnet 405Coin BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq400_85;
        consensus.llmqs[Consensus::LLMQ_100_67] = Consensus::llmq100_67_testnet;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_50_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_50_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_100_67;

        UpdateDevnetLLMQChainLocksFromArgs(args);
        UpdateDevnetLLMQInstantSendFromArgs(args);

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        miningRequiresPeers = false;
        nLLMQConnectionRetryTimeout = 60;
        m_is_mockable_chain = false;

        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

        // privKey: cVpnZj4dZvRXmBf7Jze1GjpLQb25iKP92GDXUsKdUJTXhXRo2RFA
        vSporkAddresses = {"yYhBxduZLMnancMkpzvcLFCiTgZRSk8wun"};
        nMinSporkKeys = 1;
        // devnets are started with no blocks and no MN, so we can't check for upgraded MN (as there are none)
        fBIP9CheckSmartnodesUpgraded = false;

        checkpointData = {};

        chainTxData = ChainTxData{
                static_cast<int64_t>(genesis.nTime),
                0,
                0.0
        };
    }

    void
    UpdateDevnetSubsidyAndDiffParameters(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor) {
        consensus.nMinimumDifficultyBlocks = nMinimumDifficultyBlocks;
        consensus.nHighSubsidyBlocks = nHighSubsidyBlocks;
        consensus.nHighSubsidyFactor = nHighSubsidyFactor;
    }

    void UpdateDevnetSubsidyAndDiffParametersFromArgs(const ArgsManager &args);

    void UpdateDevnetLLMQChainLocks(Consensus::LLMQType llmqType) {
        consensus.llmqTypeChainLocks = llmqType;
    }

    void UpdateDevnetLLMQChainLocksFromArgs(const ArgsManager &args);

    void UpdateDevnetLLMQInstantSend(Consensus::LLMQType llmqType) {
        consensus.llmqTypeInstantSend = llmqType;
    }

    void UpdateDevnetLLMQInstantSendFromArgs(const ArgsManager &args);
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager &args) {
        strNetworkID = CBaseChainParams::REGTEST;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nInitialSubsidy = 50 * COIN;
        consensus.nPremineSubsidy = 0;
        consensus.nPremineHeight = 0;
        consensus.nSmartnodePaymentsStartBlock = 240;
        consensus.nSmartnodePaymentsIncreaseBlock = 350;
        consensus.nSmartnodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = INT_MAX;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = INT_MAX;
        consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nSmartnodeMinimumConfirmations = 1;
        consensus.BIPCSVEnabled = true;
        consensus.BIP147Enabled = true;
        consensus.BIP34Enabled = true; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP65Enabled = true; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Enabled = true; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.DIP0001Enabled = true;
        consensus.DIP0003Enabled = true;
        consensus.DIP0008Enabled = true;
        // consensus.DIP0003EnforcementHeight = 500;
        consensus.powLimit = uint256S(
                "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPowTargetTimespan = 24 * 60 * 60; // 405Coin: 1 day
        consensus.nPowTargetSpacing = 2 * 60; // 405Coin: 2 minutes
        consensus.nMinimumDifficultyBlocks = 2000;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nPowDGWHeight = 60;
        consensus.DGWBlocksAvg = 60;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.nFutureForkBlock = 1;

        updateManager.Add
                (
                        Update(EUpdate::DEPLOYMENT_V17, std::string("v17"), 0, 10, 0, 10, 100, 10, false,
                               VoteThreshold(95, 95, 5), VoteThreshold(0, 0, 1))
                );
        updateManager.Add
                (
                        Update(EUpdate::ROUND_VOTING, std::string("Round Voting"), 1, 10, 100, 10, 100, 10, false,
                               VoteThreshold(95, 95, 5), VoteThreshold(0, 0, 1))
                );

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nCollaterals = SmartnodeCollaterals(
                {{INT_MAX, 10 * COIN}},
                {{240,     0},
                 {INT_MAX, 20}});

        pchMessageStart[0] = 0x34; // '4'
        pchMessageStart[1] = 0x30; // '0'
        pchMessageStart[2] = 0x35; // '5'
        pchMessageStart[3] = 0x52; // 'R'
        nDefaultPort = 19899;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        // UpdateVersionBitsParametersFromArgs(args);
        UpdateBudgetParametersFromArgs(args);

        genesis = CreateGenesisBlock(1762629636, 4, 0x207fffff, 4, 5000 * COIN);
        VerifyGenesisPOW(genesis);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0x94661a75e2cbfb0c1a829b1126548f9090afd27e64438d50a5612496f86ae200"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x7a4b1fc5aa80ec3f2bf6edb718b713230186c13ff16363dc4a554485e9465023"));
        consensus.nFutureRewardShare = Consensus::FutureRewardShare(0.8, 0.2, 0.0);

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fRequireRoutableExternalIP = false;
        fMineBlocksOnDemand = true;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 1; // must be lower then the LLMQ signing session timeout so that tests have control over failing behavior
        m_is_mockable_chain = true;

        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes
        nPoolMinParticipants = 2;
        nPoolNewMinParticipants = 2;
        nPoolMaxParticipants = 5;
        nPoolNewMaxParticipants = 20;

        // privKey: cVpnZj4dZvRXmBf7Jze1GjpLQb25iKP92GDXUsKdUJTXhXRo2RFA
        vSporkAddresses = {"yaackz5YDLnFuuX6gGzEs9EMRQGfqmNYjc"};
        nMinSporkKeys = 1;
        // regtest usually has no smartnodes in most tests, so don't check for upgraged MNs
        fBIP9CheckSmartnodesUpgraded = false;
        std::vector <FounderRewardStructure> rewardStructures = {{INT_MAX, 5}};// 5% founder/dev fee forever
        consensus.nFounderPayment = FounderPayment(rewardStructures, 500, "yaackz5YDLnFuuX6gGzEs9EMRQGfqmNYjc");

        checkpointData = {};

        chainTxData = ChainTxData{
                static_cast<int64_t>(genesis.nTime),
                0,
                0.0
        };

        // Regtest 405Coin addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
        // Regtest 405Coin script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);
        // Regtest private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
        // Regtest 405Coin BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        // Regtest 405Coin BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        // Regtest 405Coin BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq400_85;
        consensus.llmqs[Consensus::LLMQ_100_67] = Consensus::llmq100_67_testnet;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_50_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_50_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_100_67;
    }

    //  void
    //  UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize,
    //                              int64_t nThresholdStart, int64_t nThresholdMin, int64_t nFalloffCoeff) {
    //      consensus.vDeployments[d].nStartTime = nStartTime;
    //      consensus.vDeployments[d].nTimeout = nTimeout;
    //      if (nWindowSize != -1) {
    //          consensus.vDeployments[d].nWindowSize = nWindowSize;
    //      }
    //      if (nThresholdStart != -1) {
    //          consensus.vDeployments[d].nThresholdStart = nThresholdStart;
    //      }
    //      if (nThresholdMin != -1) {
    //          consensus.vDeployments[d].nThresholdMin = nThresholdMin;
    //      }
    //      if (nFalloffCoeff != -1) {
    //          consensus.vDeployments[d].nFalloffCoeff = nFalloffCoeff;
    //      }
    //  }

    //  void UpdateVersionBitsParametersFromArgs(const ArgsManager &args);

    void
    UpdateBudgetParameters(int nSmartnodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock) {
        consensus.nSmartnodePaymentsStartBlock = nSmartnodePaymentsStartBlock;
        consensus.nBudgetPaymentsStartBlock = nBudgetPaymentsStartBlock;
        consensus.nSuperblockStartBlock = nSuperblockStartBlock;
    }

    void UpdateBudgetParametersFromArgs(const ArgsManager &args);
};

// void CRegTestParams::UpdateVersionBitsParametersFromArgs(const ArgsManager &args) {
//     if (!args.IsArgSet("-vbparams")) return;

//     for (const std::string &strDeployment: args.GetArgs("-vbparams")) {
//         std::vector <std::string> vDeploymentParams;
//         boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
//         if (vDeploymentParams.size() != 3 && vDeploymentParams.size() != 5 && vDeploymentParams.size() != 7) {
//             throw std::runtime_error("Version bits parameters malformed, expecting "
//                                      "<deployment>:<start>:<end> or "
//                                      "<deployment>:<start>:<end>:<window>:<threshold> or "
//                                      "<deployment>:<start>:<end>:<window>:<thresholdstart>:<thresholdmin>:<falloffcoeff>");
//         }
//         int64_t nStartTime, nTimeout, nWindowSize = -1, nThresholdStart = -1, nThresholdMin = -1, nFalloffCoeff = -1;
//         if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
//             throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
//         }
//         if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
//             throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
//         }
//         if (vDeploymentParams.size() >= 5) {
//             if (!ParseInt64(vDeploymentParams[3], &nWindowSize)) {
//                 throw std::runtime_error(strprintf("Invalid nWindowSize (%s)", vDeploymentParams[3]));
//             }
//             if (!ParseInt64(vDeploymentParams[4], &nThresholdStart)) {
//                 throw std::runtime_error(strprintf("Invalid nThresholdStart (%s)", vDeploymentParams[4]));
//             }
//         }
//         if (vDeploymentParams.size() == 7) {
//             if (!ParseInt64(vDeploymentParams[5], &nThresholdMin)) {
//                 throw std::runtime_error(strprintf("Invalid nThresholdMin (%s)", vDeploymentParams[5]));
//             }
//             if (!ParseInt64(vDeploymentParams[6], &nFalloffCoeff)) {
//                 throw std::runtime_error(strprintf("Invalid nFalloffCoeff (%s)", vDeploymentParams[6]));
//             }
//         }
//         bool found = false;
//         for (int j = 0; j < (int) Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
//             if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
//                 UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout, nWindowSize,
//                                             nThresholdStart, nThresholdMin, nFalloffCoeff);
//                 found = true;
//                 LogPrintf(
//                         "Setting version bits activation parameters for %s to start=%ld, timeout=%ld, window=%ld, thresholdstart=%ld, thresholdmin=%ld, falloffcoeff=%ld\n",
//                         vDeploymentParams[0], nStartTime, nTimeout, nWindowSize, nThresholdStart, nThresholdMin,
//                         nFalloffCoeff);
//                 break;
//             }
//         }
//         if (!found) {
//             throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
//         }
//     }
// }

void CRegTestParams::UpdateBudgetParametersFromArgs(const ArgsManager &args) {
    if (!args.IsArgSet("-budgetparams")) return;

    std::string strParams = args.GetArg("-budgetparams", "");
    std::vector <std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 3) {
        throw std::runtime_error("Budget parameters malformed, expecting <smartnode>:<budget>:<superblock>");
    }
    int nSmartnodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock;
    if (!ParseInt32(vParams[0], &nSmartnodePaymentsStartBlock)) {
        throw std::runtime_error(strprintf("Invalid smartnode start height (%s)", vParams[0]));
    }
    if (!ParseInt32(vParams[1], &nBudgetPaymentsStartBlock)) {
        throw std::runtime_error(strprintf("Invalid budget start block (%s)", vParams[1]));
    }
    if (!ParseInt32(vParams[2], &nSuperblockStartBlock)) {
        throw std::runtime_error(strprintf("Invalid superblock start height (%s)", vParams[2]));
    }
    LogPrintf("Setting budget parameters to smartnode=%ld, budget=%ld, superblock=%ld\n", nSmartnodePaymentsStartBlock,
              nBudgetPaymentsStartBlock, nSuperblockStartBlock);
    UpdateBudgetParameters(nSmartnodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock);
}

void CDevNetParams::UpdateDevnetSubsidyAndDiffParametersFromArgs(const ArgsManager &args) {
    if (!args.IsArgSet("-minimumdifficultyblocks") && !args.IsArgSet("-highsubsidyblocks") &&
        !args.IsArgSet("-highsubsidyfactor"))
        return;

    int nMinimumDifficultyBlocks = gArgs.GetArg("-minimumdifficultyblocks", consensus.nMinimumDifficultyBlocks);
    int nHighSubsidyBlocks = gArgs.GetArg("-highsubsidyblocks", consensus.nHighSubsidyBlocks);
    int nHighSubsidyFactor = gArgs.GetArg("-highsubsidyfactor", consensus.nHighSubsidyFactor);
    LogPrintf("Setting minimumdifficultyblocks=%ld, highsubsidyblocks=%ld, highsubsidyfactor=%ld\n",
              nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
    UpdateDevnetSubsidyAndDiffParameters(nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
}

void CDevNetParams::UpdateDevnetLLMQChainLocksFromArgs(const ArgsManager &args)
// void UpdateBudgetParameters(int nSmartnodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
    if (!args.IsArgSet("-llmqchainlocks")) return;

    std::string strLLMQType = gArgs.GetArg("-llmqchainlocks",
                                           std::string(consensus.llmqs.at(consensus.llmqTypeChainLocks).name));
    Consensus::LLMQType llmqType = Consensus::LLMQ_NONE;
    for (const auto &p: consensus.llmqs) {
        if (p.second.name == strLLMQType) {
            llmqType = p.first;
        }
    }
    if (llmqType == Consensus::LLMQ_NONE) {
        throw std::runtime_error("Invalid LLMQ type specified for -llmqchainlocks.");
    }
    LogPrintf("Setting llmqchainlocks to size=%ld\n", llmqType);
    UpdateDevnetLLMQChainLocks(llmqType);
}

void CDevNetParams::UpdateDevnetLLMQInstantSendFromArgs(const ArgsManager &args) {
    if (!args.IsArgSet("-llmqinstantsend")) return;

    std::string strLLMQType = gArgs.GetArg("-llmqinstantsend",
                                           std::string(consensus.llmqs.at(consensus.llmqTypeInstantSend).name));
    Consensus::LLMQType llmqType = Consensus::LLMQ_NONE;
    for (const auto &p: consensus.llmqs) {
        if (p.second.name == strLLMQType) {
            llmqType = p.first;
        }
    }
    if (llmqType == Consensus::LLMQ_NONE) {
        throw std::runtime_error("Invalid LLMQ type specified for -llmqinstantsend.");
    }
    LogPrintf("Setting llmqinstantsend to size=%ld\n", llmqType);
    UpdateDevnetLLMQInstantSend(llmqType);
}

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

UpdateManager &Updates() {
    assert(globalChainParams);
    return globalChainParams->Updates();
}


std::unique_ptr <CChainParams> CreateChainParams(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN)
        return std::make_unique<CMainParams>();
    else if (chain == CBaseChainParams::TESTNET)
        return std::make_unique<CTestNetParams>();
    else if (chain == CBaseChainParams::DEVNET) {
        return std::make_unique<CDevNetParams>(gArgs);
    } else if (chain == CBaseChainParams::REGTEST)
        return std::make_unique<CRegTestParams>(gArgs);

    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string &network) {
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateLLMQParams(size_t totalMnCount, int height, const CBlockIndex* blockIndex, bool lowLLMQParams) {
    globalChainParams->UpdateLLMQParams(totalMnCount, height, blockIndex, lowLLMQParams);
}

bool IsMiningPhase(const Consensus::LLMQParams &params, int nHeight) {
    int phaseIndex = nHeight % params.dkgInterval;
    if (phaseIndex >= params.dkgMiningWindowStart && phaseIndex <= params.dkgMiningWindowEnd) {
        return true;
    }
    return false;
}

bool IsLLMQsMiningPhase(int nHeight) {
    for (auto &it: globalChainParams->GetConsensus().llmqs) {
        if (IsMiningPhase(it.second, nHeight)) {
            return true;
        }
    }
    return false;
}

void CChainParams::UpdateLLMQParams(size_t totalMnCount, int height, const CBlockIndex* blockIndex, bool lowLLMQParams) {
    bool isNotLLMQsMiningPhase;
    if (lastCheckHeight < height && (lastCheckMnCount != totalMnCount || lastCheckedLowLLMQParams != lowLLMQParams) &&
        (isNotLLMQsMiningPhase = !IsLLMQsMiningPhase(height))) {
        LogPrintf("---UpdateLLMQParams %d-%d-%ld-%ld-%d\n", lastCheckHeight, height, lastCheckMnCount, totalMnCount,
                  isNotLLMQsMiningPhase);
        lastCheckMnCount = totalMnCount;
        lastCheckedLowLLMQParams = lowLLMQParams;
        lastCheckHeight = height;
        bool isTestNet = strcmp(Params().NetworkIDString().c_str(), "test") == 0;
        if (totalMnCount < 5) {
            consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq3_60;
            if (isTestNet) {
                consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq5_60;
                consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq5_85;
            } else {
                consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq20_60;
                consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq20_85;
            }
        } else if ((totalMnCount < 80 && isTestNet) || (totalMnCount < 100 && !isTestNet)) {
            consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq10_60;
            consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq20_60;
            consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq20_85;
        } else if ((((height >= 24280 && totalMnCount < 600) || (height < 24280 && totalMnCount < 4000)) && isTestNet)
                   || (totalMnCount < 600 && !isTestNet)) {
            consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq50_60;
            consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq40_60;
            consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq40_85;
        } else {
            consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq50_60;
            if (Updates().IsActive(EUpdate::QUORUMS_200_8, blockIndex)) {
               consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq200_60;
               consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq200_85;
            } else {
               consensus.llmqs[Consensus::LLMQ_400_60] = Consensus::llmq400_60;
               consensus.llmqs[Consensus::LLMQ_400_85] = Consensus::llmq400_85;
            }
        }
        if (lowLLMQParams) {
            consensus.llmqs[Consensus::LLMQ_50_60] = Consensus::llmq200_2;
        }
    }

}
