#include "subghz_tx.h"

#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/types.h>
#include <lib/flipper_format/flipper_format.h>

#include <string.h>

#define TAG "SubGhzBfTx"

struct SubGhzTx {
    Storage* storage;
    SubGhzEnvironment* environment;
    const SubGhzDevice* device;
};

SubGhzTx* subghz_tx_alloc(void) {
    SubGhzTx* instance = malloc(sizeof(SubGhzTx));
    instance->storage = furi_record_open(RECORD_STORAGE);

    subghz_devices_init();
    instance->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

    instance->environment = subghz_environment_alloc();
    subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_NAME);
    subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
    subghz_environment_set_came_atomo_rainbow_table_file_name(
        instance->environment, SUBGHZ_CAME_ATOMO_DIR_NAME);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        instance->environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        instance->environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)&subghz_protocol_registry);

    return instance;
}

void subghz_tx_free(SubGhzTx* instance) {
    furi_assert(instance);
    subghz_environment_free(instance->environment);
    subghz_devices_deinit();
    furi_record_close(RECORD_STORAGE);
    free(instance);
}

bool subghz_tx_session_begin(SubGhzTx* instance) {
    furi_assert(instance);
    if(!instance->device) return false;
    subghz_devices_begin(instance->device);
    return true;
}

void subghz_tx_session_end(SubGhzTx* instance) {
    furi_assert(instance);
    if(!instance->device) return;
    subghz_devices_idle(instance->device);
    subghz_devices_sleep(instance->device);
    subghz_devices_end(instance->device);
}

static FuriHalSubGhzPreset subghz_tx_preset_from_name(const char* name, bool* is_custom) {
    *is_custom = false;
    if(strcmp(name, "FuriHalSubGhzPresetOok270Async") == 0)
        return FuriHalSubGhzPresetOok270Async;
    if(strcmp(name, "FuriHalSubGhzPresetOok650Async") == 0)
        return FuriHalSubGhzPresetOok650Async;
    if(strcmp(name, "FuriHalSubGhzPreset2FSKDev238Async") == 0)
        return FuriHalSubGhzPreset2FSKDev238Async;
    if(strcmp(name, "FuriHalSubGhzPreset2FSKDev476Async") == 0)
        return FuriHalSubGhzPreset2FSKDev476Async;
    if(strcmp(name, "FuriHalSubGhzPresetMSK99_97KbAsync") == 0)
        return FuriHalSubGhzPresetMSK99_97KbAsync;
    if(strcmp(name, "FuriHalSubGhzPresetGFSK9_99KbAsync") == 0)
        return FuriHalSubGhzPresetGFSK9_99KbAsync;
    // Anything else is treated as a custom preset carried in "Custom_preset_data".
    *is_custom = true;
    return FuriHalSubGhzPresetCustom;
}

static SubGhzTxResult subghz_tx_wait_complete(
    SubGhzTx* instance,
    volatile bool* stop) {
    SubGhzTxResult result = SubGhzTxResultOk;
    while(!subghz_devices_is_async_complete_tx(instance->device)) {
        if(stop && *stop) {
            result = SubGhzTxResultStopped;
            break;
        }
        furi_delay_ms(10);
    }
    subghz_devices_stop_async_tx(instance->device);
    return result;
}

SubGhzTxResult subghz_tx_transmit_file(
    SubGhzTx* instance,
    const char* path,
    SubGhzTxFileInfo* info,
    volatile bool* stop) {
    furi_assert(instance);
    if(!instance->device) return SubGhzTxResultErrorTx;

    SubGhzTxResult result = SubGhzTxResultOk;
    FlipperFormat* fff = flipper_format_file_alloc(instance->storage);
    FuriString* temp_str = furi_string_alloc();
    FuriString* preset_str = furi_string_alloc();
    FuriString* protocol_str = furi_string_alloc();
    uint8_t* preset_data = NULL;
    uint32_t frequency = 0;

    do {
        if(!flipper_format_file_open_existing(fff, path)) {
            result = SubGhzTxResultErrorOpen;
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(fff, temp_str, &version)) {
            result = SubGhzTxResultErrorHeader;
            break;
        }
        if(!flipper_format_read_uint32(fff, "Frequency", &frequency, 1)) {
            result = SubGhzTxResultErrorFrequency;
            break;
        }
        if(!subghz_devices_is_frequency_valid(instance->device, frequency)) {
            result = SubGhzTxResultErrorFrequency;
            break;
        }
        if(!flipper_format_read_string(fff, "Preset", preset_str)) {
            result = SubGhzTxResultErrorPreset;
            break;
        }
        if(!flipper_format_read_string(fff, "Protocol", protocol_str)) {
            result = SubGhzTxResultErrorProtocol;
            break;
        }

        if(info) {
            info->frequency = frequency;
            strncpy(info->protocol, furi_string_get_cstr(protocol_str), sizeof(info->protocol) - 1);
            info->protocol[sizeof(info->protocol) - 1] = '\0';
            strncpy(info->preset, furi_string_get_cstr(preset_str), sizeof(info->preset) - 1);
            info->preset[sizeof(info->preset) - 1] = '\0';
        }

        bool is_custom = false;
        FuriHalSubGhzPreset preset =
            subghz_tx_preset_from_name(furi_string_get_cstr(preset_str), &is_custom);
        if(is_custom) {
            // Custom_preset_data appears before Protocol in the file, and flipper_format
            // only scans forward, so rewind before looking it up.
            flipper_format_rewind(fff);
            uint32_t count = 0;
            if(!flipper_format_get_value_count(fff, "Custom_preset_data", &count) || count == 0) {
                result = SubGhzTxResultErrorPreset;
                break;
            }
            preset_data = malloc(count);
            if(!flipper_format_read_hex(fff, "Custom_preset_data", preset_data, count)) {
                result = SubGhzTxResultErrorPreset;
                break;
            }
        }

        subghz_devices_reset(instance->device);
        subghz_devices_idle(instance->device);
        subghz_devices_load_preset(instance->device, preset, preset_data);
        subghz_devices_set_frequency(instance->device, frequency);

        bool is_raw = (furi_string_cmp_str(protocol_str, "RAW") == 0);

        if(is_raw) {
            // The RAW worker reopens the file itself, so release our handle first.
            flipper_format_free(fff);
            fff = NULL;

            SubGhzFileEncoderWorker* worker = subghz_file_encoder_worker_alloc();
            if(!subghz_file_encoder_worker_start(
                   worker, path, subghz_devices_get_name(instance->device))) {
                subghz_file_encoder_worker_free(worker);
                result = SubGhzTxResultErrorTx;
                break;
            }
            // Give the worker a moment to prime its sample buffer.
            furi_delay_ms(100);

            if(!subghz_devices_start_async_tx(
                   instance->device, subghz_file_encoder_worker_get_level_duration, worker)) {
                subghz_file_encoder_worker_stop(worker);
                subghz_file_encoder_worker_free(worker);
                result = SubGhzTxResultErrorTx;
                break;
            }
            result = subghz_tx_wait_complete(instance, stop);
            subghz_file_encoder_worker_stop(worker);
            subghz_file_encoder_worker_free(worker);
        } else {
            SubGhzTransmitter* transmitter = subghz_transmitter_alloc_init(
                instance->environment, furi_string_get_cstr(protocol_str));
            if(!transmitter) {
                result = SubGhzTxResultErrorProtocol;
                break;
            }
            flipper_format_rewind(fff);
            SubGhzProtocolStatus status = subghz_transmitter_deserialize(transmitter, fff);
            if(status != SubGhzProtocolStatusOk) {
                subghz_transmitter_free(transmitter);
                result = SubGhzTxResultErrorDeserialize;
                break;
            }
            if(!subghz_devices_start_async_tx(
                   instance->device, subghz_transmitter_yield, transmitter)) {
                subghz_transmitter_free(transmitter);
                result = SubGhzTxResultErrorTx;
                break;
            }
            result = subghz_tx_wait_complete(instance, stop);
            subghz_transmitter_free(transmitter);
        }
    } while(false);

    if(instance->device) subghz_devices_idle(instance->device);

    if(fff) flipper_format_free(fff);
    if(preset_data) free(preset_data);
    furi_string_free(temp_str);
    furi_string_free(preset_str);
    furi_string_free(protocol_str);
    return result;
}

const char* subghz_tx_result_str(SubGhzTxResult result) {
    switch(result) {
    case SubGhzTxResultOk:
        return "OK";
    case SubGhzTxResultErrorOpen:
        return "Open err";
    case SubGhzTxResultErrorHeader:
        return "Bad file";
    case SubGhzTxResultErrorFrequency:
        return "Freq err";
    case SubGhzTxResultErrorPreset:
        return "Preset err";
    case SubGhzTxResultErrorProtocol:
        return "Proto err";
    case SubGhzTxResultErrorDeserialize:
        return "Parse err";
    case SubGhzTxResultErrorTx:
        return "TX err";
    case SubGhzTxResultStopped:
        return "Stopped";
    default:
        return "?";
    }
}
