/*
Copyright (C) 2022 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#pragma once

#include <stdint.h>

#include "engine_handle.h"
#include "scap_open.h"

#ifdef __cplusplus
extern "C" {
#endif

struct scap_stats;
typedef struct scap scap_t;
typedef struct ppm_evt_hdr scap_evt;

/*
 * magic constants, matching the kmod-only ioctl numbers defined as:
#define PPM_IOCTL_MAGIC	's'
#define PPM_IOCTL_MASK_ZERO_EVENTS _IO(PPM_IOCTL_MAGIC, 5)
#define PPM_IOCTL_MASK_SET_EVENT   _IO(PPM_IOCTL_MAGIC, 6)
#define PPM_IOCTL_MASK_UNSET_EVENT _IO(PPM_IOCTL_MAGIC, 7)
 */
enum scap_eventmask_op {
	SCAP_EVENTMASK_ZERO = 0x7305, //< disable all events
	SCAP_EVENTMASK_SET = 0x7306, //< enable an event
	SCAP_EVENTMASK_UNSET = 0x7307, //< disable an event
};

/**
 * @brief settings configurable for scap engines
 */
enum scap_setting {
	/**
	 * @brief sampling ratio
	 * arg1: sampling ratio (power of 2, <= 128)
	 * arg2: dropping mode enabled (1) / disabled (0)
	 */
	SCAP_SAMPLING_RATIO,
	/**
	 * @brief control tracers capture
	 * arg1: enabled?
	 */
	SCAP_TRACERS_CAPTURE,
	/**
	 * @brief instrument page faults
	 * arg1: enabled?
	 */
	SCAP_PAGE_FAULTS,
	/**
	 * @brief length of captured data buffers
	 * arg1: the length (< 65536)
	 */
	SCAP_SNAPLEN,
	/**
	 * @brief enable/disable individual events
	 * arg1: scap_eventmask_op
	 * arg2: event id (ignored for SCAP_EVENTMASK_ZERO)
	 */
	SCAP_EVENTMASK,
	/**
	 * @brief enable/disable dynamic snaplen
	 * arg1: enabled?
	 */
	SCAP_DYNAMIC_SNAPLEN,
	/**
	 * @brief simple driver mode
	 * arg1: enabled?
	 */
	SCAP_SIMPLEDRIVER_MODE,
	/**
	 * @brief full capture port range
	 * arg1: min port
	 * arg2: max port
	 */
	SCAP_FULLCAPTURE_PORT_RANGE,
	/**
	 * @brief port for statsd metrics
	 * arg1: statsd port
	 */
	SCAP_STATSD_PORT,
};

struct scap_vtable {
	/**
	 * @brief name of the engine
	 */
	const char* name;

	/**
	 * @brief one of the SCAP_MODE_* constants, designating the purpose
	 * of the engine (live capture, capture files, etc.)
	 */
	scap_mode_t mode;

	/**
	 * @brief check whether `open_args` are compatible with this engine
	 * @param open_args a scap open request structure
	 * @return true if this engine can handle `open_args`, false otherwise
	 *
	 * If this field is NULL, only `open_args->mode` is checked against
	 * the `mode` field. If it is *not* NULL, the mode is checked before
	 * calling `match()`.
	 */
	bool (*match)(scap_open_args* open_args);

	/**
	 * @brief allocate an engine-specific handle
	 * @param main_handle pointer to the main scap_t handle
	 * @param lasterr_ptr pointer to a SCAP_LASTERR_SIZE buffer
	 *                    for error messages, can be stored
	 *                    in the engine handle for easier access
	 * @return pointer to the newly allocated handle or NULL
	 */
	SCAP_HANDLE_T* (*alloc_handle)(scap_t* main_handle, char *lasterr_ptr);

	/**
	 * @brief perform engine-specific initialization
	 * @param main_handle pointer to the main scap_t handle
	 * @param open_args a scap open request structure
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*init)(scap_t* main_handle, scap_open_args* open_args);

	/**
	 * @brief free the engine-specific handle
	 * @param engine wraps the pointer to the engine-specific handle
	 */
	void (*free_handle)(struct scap_engine_handle engine);

	/**
	 * @brief close the engine
	 * @param engine wraps the pointer to the engine-specific handle
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*close)(struct scap_engine_handle engine);

	/**
	 * @brief fetch the next event
	 * @param engine wraps the pointer to the engine-specific handle
	 * @param pevent [out] where the pointer to the next event gets stored
	 * @param pcpuid [out] where the CPU on which the event was received
	 *               gets stored
	 * @return SCAP_SUCCESS or a failure code
	 *
	 * SCAP_SUCCESS: event successfully returned and stored in *pevent
	 * SCAP_FAILURE: an error occurred
	 * SCAP_TIMEOUT: no events arrived for a while (not an error)
	 * SCAP_EOF: no more events are going to arrive
	 *
	 * The memory pointed to by *pevent must be owned by the engine
	 * and must remain valid at least until the next call to next()
	 */
	int32_t (*next)(struct scap_engine_handle engine, scap_evt **pevent, uint16_t *pcpuid);

	/**
	 * @brief start a capture
	 * @param engine
	 * @param engine wraps the pointer to the engine-specific handle
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*start_capture)(struct scap_engine_handle engine);

	/**
	 * @brief stop a running capture
	 * @param engine wraps the pointer to the engine-specific handle
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*stop_capture)(struct scap_engine_handle engine);

	/**
	 * @brief change engine settings
	 * @param engine wraps the pointer to the engine-specific handle
	 * @param setting the setting to change
	 * @param arg1 setting-specific value
	 * @param arg2 setting-specific value
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*configure)(struct scap_engine_handle engine, enum scap_setting setting, unsigned long arg1, unsigned long arg2);

	/**
	 * @brief get engine statistics
	 * @param engine wraps the pointer to the engine-specific handle
	 * @param stats the stats struct to be filled
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*get_stats)(struct scap_engine_handle engine, struct scap_stats *stats);

	/**
	 * @brief get the number of tracepoint hits
	 * @param engine wraps the pointer to the engine-specific handle
	 * @param ret [out] the number of hits
	 * @return SCAP_SUCCESS or a failure code
	 */
	int32_t (*get_n_tracepoint_hit)(struct scap_engine_handle engine, long *ret);

	/**
	 * @brief get the number of used devices
	 * @param engine wraps the pointer to the engine-specific handle
	 * @return the number of used devices
	 */
	uint32_t (*get_n_devs)(struct scap_engine_handle engine);

	/**
	 * @brief get the maximum buffer space used so far
	 * @param engine wraps the pointer to the engine-specific handle
	 * @return the buffer space used, in bytes
	 */
	uint64_t (*get_max_buf_used)(struct scap_engine_handle engine);
};

#ifdef __cplusplus
}
#endif
