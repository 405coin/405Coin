// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RAPTOREUMCONSENSUS_H
#define BITCOIN_RAPTOREUMCONSENSUS_H

#include <stdint.h>

#if defined(BUILD_BITCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include <config/405Coin-config.h>
#if defined(_WIN32)
#if defined(HAVE_DLLEXPORT_ATTRIBUTE)
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL
#endif
#elif defined(HAVE_DEFAULT_VISIBILITY_ATTRIBUTE)
#define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
#endif
#elif defined(MSC_VER) && !defined(STATIC_LIBRAPTOREUMCONSENSUS)
#define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
#define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RAPTOREUMCONSENSUS_API_VER 0

typedef enum coin405consensus_error_t {
    coin405consensus_ERR_OK = 0,
    coin405consensus_ERR_TX_INDEX,
    coin405consensus_ERR_TX_SIZE_MISMATCH,
    coin405consensus_ERR_TX_DESERIALIZE,
    coin405consensus_ERR_INVALID_FLAGS,
} coin405consensus_error;

/** Script verification flags */
enum {
    coin405consensus_SCRIPT_FLAGS_VERIFY_NONE = 0,
    coin405consensus_SCRIPT_FLAGS_VERIFY_P2SH = (1U << 0), // evaluate P2SH (BIP16) subscripts
    coin405consensus_SCRIPT_FLAGS_VERIFY_DERSIG = (1U << 2), // enforce strict DER (BIP66) compliance
    coin405consensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY = (1U << 4), // enforce NULLDUMMY (BIP147)
    coin405consensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9), // enable CHECKLOCKTIMEVERIFY (BIP65)
    coin405consensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10), // enable CHECKSEQUENCEVERIFY (BIP112)
    coin405consensus_SCRIPT_FLAGS_VERIFY_ALL =
    coin405consensus_SCRIPT_FLAGS_VERIFY_P2SH | coin405consensus_SCRIPT_FLAGS_VERIFY_DERSIG |
    coin405consensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY | coin405consensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
    coin405consensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not nullptr, err will contain an error/success code for the operation
EXPORT_SYMBOL int coin405consensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                                   const unsigned char *txTo, unsigned int txToLen,
                                                   unsigned int nIn, unsigned int flags, coin405consensus_error *err);

EXPORT_SYMBOL unsigned int coin405consensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // BITCOIN_RAPTOREUMCONSENSUS_H
