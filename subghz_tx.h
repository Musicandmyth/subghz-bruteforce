#pragma once

#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzBfTx SubGhzBfTx;

typedef enum {
    SubGhzTxResultOk,
    SubGhzTxResultErrorOpen,
    SubGhzTxResultErrorHeader,
    SubGhzTxResultErrorFrequency,
    SubGhzTxResultErrorPreset,
    SubGhzTxResultErrorProtocol,
    SubGhzTxResultErrorDeserialize,
    SubGhzTxResultErrorTx,
    SubGhzTxResultStopped,
} SubGhzTxResult;

/** Metadata read from a .sub file, filled in for UI display. */
typedef struct {
    uint32_t frequency;
    char protocol[36];
    char preset[40];
} SubGhzTxFileInfo;

/** Allocate the transmit engine (opens storage, radio device and keystore). */
SubGhzBfTx* subghz_tx_alloc(void);

/** Free the transmit engine. */
void subghz_tx_free(SubGhzBfTx* instance);

/** Power up the radio for a transmit session. Call once before a batch of files. */
bool subghz_tx_session_begin(SubGhzBfTx* instance);

/** Put the radio back to sleep. Call once after a batch of files. */
void subghz_tx_session_end(SubGhzBfTx* instance);

/**
 * Transmit a single .sub file (blocking until the transmission completes).
 * @param instance  engine instance
 * @param path      full path to the .sub file
 * @param info      optional, filled with parsed metadata (may be NULL)
 * @param stop      optional abort flag, polled during transmission (may be NULL)
 */
SubGhzTxResult subghz_tx_transmit_file(
    SubGhzBfTx* instance,
    const char* path,
    SubGhzTxFileInfo* info,
    volatile bool* stop);

/** Human-readable name for a result code. */
const char* subghz_tx_result_str(SubGhzTxResult result);

#ifdef __cplusplus
}
#endif
