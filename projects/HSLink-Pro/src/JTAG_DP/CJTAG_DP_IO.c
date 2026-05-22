/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "DAP_config.h"
#include "DAP.h"


#if (DAP_JTAG != 0)

#define CJTAG_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)

static void cjtag_clock_cycle(void)
{
    PIN_SWCLK_TCK_CLR();
    CJTAG_DELAY();
    PIN_SWCLK_TCK_SET();
    CJTAG_DELAY();
    PIN_SWCLK_TCK_CLR();
    CJTAG_DELAY();
}

static void cjtag_write_tmsc_cycle(uint32_t bit)
{
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(bit);
    cjtag_clock_cycle();
}

static uint32_t cjtag_read_tmsc_cycle(void)
{
    uint32_t bit;

    PIN_SWDIO_OUT_DISABLE();
    PIN_SWCLK_TCK_CLR();
    CJTAG_DELAY();
    PIN_SWCLK_TCK_SET();
    CJTAG_DELAY();
    bit = PIN_SWDIO_IN();
    PIN_SWCLK_TCK_CLR();
    CJTAG_DELAY();
    PIN_SWDIO_OUT_ENABLE();

    return bit & 1U;
}

static uint32_t cjtag_oscan1_cycle(uint32_t tms, uint32_t tdi)
{
    uint32_t tdo;

    /* OScan1 transfers one JTAG cycle as three TCKC slots: nTDI, TMS, TDO. */
    cjtag_write_tmsc_cycle((tdi ^ 1U) & 1U);
    cjtag_write_tmsc_cycle(tms & 1U);
    tdo = cjtag_read_tmsc_cycle();

    return tdo;
}

static void cjtag_standard_tms_cycle(uint32_t bit)
{
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(bit);
    cjtag_clock_cycle();
}

static void cjtag_standard_tms_sequence(const uint8_t *bits, uint32_t count)
{
    uint32_t n;

    for (n = 0U; n < count; n++) {
        cjtag_standard_tms_cycle(bits[n]);
    }
}

static void cjtag_escape_reset(void)
{
    uint32_t n;

    PIN_SWCLK_TCK_CLR();
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(0U);
    CJTAG_DELAY();

    for (n = 0U; n < 8U; n++) {
        PIN_SWDIO_OUT(1U);
        CJTAG_DELAY();
        PIN_SWDIO_OUT(0U);
        CJTAG_DELAY();
    }
}

static void cjtag_tap1_reset_to_idle(void)
{
    static const uint8_t reset_to_idle[] = {1U, 1U, 1U, 1U, 1U, 0U};

    cjtag_standard_tms_sequence(reset_to_idle, sizeof(reset_to_idle));
}

static void cjtag_tap7_zero_bit_scan(void)
{
    static const uint8_t zbs[] = {1U, 0U, 1U, 1U, 0U};

    cjtag_standard_tms_sequence(zbs, sizeof(zbs));
}

static void cjtag_tap7_lock(void)
{
    static const uint8_t lock[] = {1U, 0U, 0U, 1U, 1U, 0U};

    cjtag_standard_tms_sequence(lock, sizeof(lock));
}

static void cjtag_tap7_stfmt_section(uint32_t shift_count)
{
    uint32_t n;

    cjtag_standard_tms_cycle(1U);       /* Select-DR-Scan */
    cjtag_standard_tms_cycle(0U);       /* Capture-DR */

    for (n = 0U; n <= shift_count; n++) {
        cjtag_standard_tms_cycle(0U);   /* Shift-DR */
    }

    cjtag_standard_tms_cycle(1U);       /* Exit1-DR */
    cjtag_standard_tms_cycle(1U);       /* Update-DR */
    cjtag_standard_tms_cycle(0U);       /* Idle */
}

static void cjtag_tap7_check_packet_end(void)
{
    uint32_t n;

    for (n = 0U; n < 4U; n++) {
        cjtag_standard_tms_cycle(0U);
    }
}

static void cjtag_exit_control_mode(void)
{
    static const uint8_t exit_control_tms[] = {0U, 1U, 1U, 0U, 1U, 1U};
    uint32_t n;

    for (n = 0U; n < sizeof(exit_control_tms); n++) {
        (void)cjtag_oscan1_cycle(exit_control_tms[n], 0U);
    }
}

static void cjtag_activate_oscan1(void)
{
    cjtag_escape_reset();
    cjtag_tap1_reset_to_idle();
    cjtag_tap7_zero_bit_scan();
    cjtag_tap7_zero_bit_scan();
    cjtag_tap7_lock();
    cjtag_tap7_stfmt_section(3U);
    cjtag_tap7_stfmt_section(9U);
    cjtag_tap7_check_packet_end();
    cjtag_exit_control_mode();

    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);
}

void CJTAG_PORT_JTAG_SETUP(void)
{
    HPM_IOC->PAD[PIN_TCK].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0) | IOC_PAD_FUNC_CTL_LOOP_BACK_MASK;
    HPM_IOC->PAD[PIN_TMS].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    HPM_IOC->PAD[PIN_TDI].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    HPM_IOC->PAD[PIN_TDO].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    HPM_IOC->PAD[PIN_JTAG_TRST].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    HPM_IOC->PAD[PIN_SRST].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);

    HPM_IOC->PAD[PIN_TCK].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1) | IOC_PAD_PAD_CTL_SPD_SET(3);
    HPM_IOC->PAD[PIN_TMS].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1) | IOC_PAD_PAD_CTL_SPD_SET(3);
    HPM_IOC->PAD[PIN_TDI].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2);
    HPM_IOC->PAD[PIN_TDO].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2);
    HPM_IOC->PAD[PIN_JTAG_TRST].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1) | IOC_PAD_PAD_CTL_SPD_SET(3);
    HPM_IOC->PAD[PIN_SRST].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1) | IOC_PAD_PAD_CTL_SPD_SET(3);

#ifdef PIN_LED_RUNNING
    HPM_IOC->PAD[PIN_LED_RUNNING].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    HPM_IOC->PAD[PIN_LED_RUNNING].PAD_CTL = IOC_PAD_PAD_CTL_PRS_SET(2) | IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1) | IOC_PAD_PAD_CTL_SPD_SET(3);
    gpiom_configure_pin_control_setting(PIN_LED_RUNNING);
    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_LED_RUNNING), GPIO_GET_PIN_INDEX(PIN_LED_RUNNING));
#endif

#ifdef PIN_LED_CONNECTED
    HPM_IOC->PAD[PIN_LED_CONNECTED].FUNC_CTL = IOC_PAD_FUNC_CTL_ALT_SELECT_SET(0);
    gpiom_configure_pin_control_setting(PIN_LED_CONNECTED);
    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_LED_CONNECTED), GPIO_GET_PIN_INDEX(PIN_LED_CONNECTED));
#endif

    gpiom_configure_pin_control_setting(PIN_TCK);
    gpiom_configure_pin_control_setting(PIN_TMS);
    gpiom_configure_pin_control_setting(PIN_TDI);
    gpiom_configure_pin_control_setting(PIN_TDO);
    gpiom_configure_pin_control_setting(PIN_JTAG_TRST);
    gpiom_configure_pin_control_setting(PIN_SRST);

    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_TCK), GPIO_GET_PIN_INDEX(PIN_TCK));
    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_TMS), GPIO_GET_PIN_INDEX(PIN_TMS));
    gpio_set_pin_input(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_TDI), GPIO_GET_PIN_INDEX(PIN_TDI));
    gpio_set_pin_input(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_TDO), GPIO_GET_PIN_INDEX(PIN_TDO));
    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_JTAG_TRST), GPIO_GET_PIN_INDEX(PIN_JTAG_TRST));
    gpio_set_pin_output(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_SRST), GPIO_GET_PIN_INDEX(PIN_SRST));

    gpio_write_pin(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_SRST), GPIO_GET_PIN_INDEX(PIN_SRST), !HSLink_Global.reset_level);
    gpio_write_pin(PIN_GPIO, GPIO_GET_PORT_INDEX(PIN_JTAG_TRST), GPIO_GET_PIN_INDEX(PIN_JTAG_TRST), 1);

    PIN_SWCLK_TCK_SET();
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);

    cjtag_activate_oscan1();
}

void CJTAG_JTAG_Sequence(uint32_t info, const uint8_t *tdi, uint8_t *tdo)
{
    uint32_t i_val;
    uint32_t o_val;
    uint32_t bit;
    uint32_t tms;
    uint32_t n, k;

    n = info & JTAG_SEQUENCE_TCK;
    if (n == 0U) {
        n = 64U;
    }

    tms = (info & JTAG_SEQUENCE_TMS) ? 1U : 0U;

    while (n) {
        i_val = *tdi++;
        o_val = 0U;
        for (k = 8U; k && n; k--, n--) {
            bit = cjtag_oscan1_cycle(tms, i_val);
            i_val >>= 1;
            o_val >>= 1;
            o_val |= bit << 7;
        }
        o_val >>= k;
        if (info & JTAG_SEQUENCE_TDO) {
            *tdo++ = (uint8_t)o_val;
        }
    }
}

void CJTAG_JTAG_IR(uint32_t ir)
{
    uint32_t n;

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Select-DR-Scan */
    (void)cjtag_oscan1_cycle(1U, 1U);    /* Select-IR-Scan */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Capture-IR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Shift-IR */

    for (n = DAP_Data.jtag_dev.ir_before[DAP_Data.jtag_dev.index]; n; n--) {
        (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass before data */
    }
    for (n = DAP_Data.jtag_dev.ir_length[DAP_Data.jtag_dev.index] - 1U; n; n--) {
        (void)cjtag_oscan1_cycle(0U, ir); /* Set IR bits (except last) */
        ir >>= 1;
    }
    n = DAP_Data.jtag_dev.ir_after[DAP_Data.jtag_dev.index];
    if (n) {
        (void)cjtag_oscan1_cycle(0U, ir); /* Set last IR bit */
        for (--n; n; n--) {
            (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass after data */
        }
        (void)cjtag_oscan1_cycle(1U, 1U); /* Bypass & Exit1-IR */
    } else {
        (void)cjtag_oscan1_cycle(1U, ir); /* Set last IR bit & Exit1-IR */
    }

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Update-IR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Idle */
}

uint32_t CJTAG_JTAG_ReadIDCode(void)
{
    uint32_t bit;
    uint32_t val;
    uint32_t n;

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Select-DR-Scan */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Capture-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Shift-DR */

    for (n = DAP_Data.jtag_dev.index; n; n--) {
        (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass before data */
    }

    val = 0U;
    for (n = 31U; n; n--) {
        bit = cjtag_oscan1_cycle(0U, 1U); /* Get D0..D30 */
        val |= bit << 31;
        val >>= 1;
    }
    bit = cjtag_oscan1_cycle(1U, 1U);     /* Get D31 & Exit1-DR */
    val |= bit << 31;

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Update-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Idle */

    return val;
}

void CJTAG_JTAG_WriteAbort(uint32_t data)
{
    uint32_t n;

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Select-DR-Scan */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Capture-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Shift-DR */

    for (n = DAP_Data.jtag_dev.index; n; n--) {
        (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass before data */
    }

    (void)cjtag_oscan1_cycle(0U, 0U);    /* Set RnW=0 (Write) */
    (void)cjtag_oscan1_cycle(0U, 0U);    /* Set A2=0 */
    (void)cjtag_oscan1_cycle(0U, 0U);    /* Set A3=0 */

    for (n = 31U; n; n--) {
        (void)cjtag_oscan1_cycle(0U, data); /* Set D0..D30 */
        data >>= 1;
    }
    n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;
    if (n) {
        (void)cjtag_oscan1_cycle(0U, data); /* Set D31 */
        for (--n; n; n--) {
            (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass after data */
        }
        (void)cjtag_oscan1_cycle(1U, 1U); /* Bypass & Exit1-DR */
    } else {
        (void)cjtag_oscan1_cycle(1U, data); /* Set D31 & Exit1-DR */
    }

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Update-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Idle */
}

uint8_t CJTAG_JTAG_Transfer(uint32_t request, uint32_t *data)
{
    uint32_t ack;
    uint32_t bit;
    uint32_t val;
    uint32_t n;

    (void)cjtag_oscan1_cycle(1U, 1U);    /* Select-DR-Scan */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Capture-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Shift-DR */

    for (n = DAP_Data.jtag_dev.index; n; n--) {
        (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass before data */
    }

    bit = cjtag_oscan1_cycle(0U, request >> 1); /* Set RnW, Get ACK.0 */
    ack = bit << 1;
    bit = cjtag_oscan1_cycle(0U, request >> 2); /* Set A2,  Get ACK.1 */
    ack |= bit << 0;
    bit = cjtag_oscan1_cycle(0U, request >> 3); /* Set A3,  Get ACK.2 */
    ack |= bit << 2;

    if (ack != DAP_TRANSFER_OK) {
        (void)cjtag_oscan1_cycle(1U, 1U); /* Exit1-DR */
        goto exit;
    }

    if (request & DAP_TRANSFER_RnW) {
        val = 0U;
        for (n = 31U; n; n--) {
            bit = cjtag_oscan1_cycle(0U, 1U); /* Get D0..D30 */
            val |= bit << 31;
            val >>= 1;
        }
        n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;
        if (n) {
            bit = cjtag_oscan1_cycle(0U, 1U); /* Get D31 */
            for (--n; n; n--) {
                (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass after data */
            }
            (void)cjtag_oscan1_cycle(1U, 1U); /* Bypass & Exit1-DR */
        } else {
            bit = cjtag_oscan1_cycle(1U, 1U); /* Get D31 & Exit1-DR */
        }
        val |= bit << 31;
        if (data) {
            *data = val;
        }
    } else {
        val = *data;
        for (n = 31U; n; n--) {
            (void)cjtag_oscan1_cycle(0U, val); /* Set D0..D30 */
            val >>= 1;
        }
        n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;
        if (n) {
            (void)cjtag_oscan1_cycle(0U, val); /* Set D31 */
            for (--n; n; n--) {
                (void)cjtag_oscan1_cycle(0U, 1U); /* Bypass after data */
            }
            (void)cjtag_oscan1_cycle(1U, 1U); /* Bypass & Exit1-DR */
        } else {
            (void)cjtag_oscan1_cycle(1U, val); /* Set D31 & Exit1-DR */
        }
    }

exit:
    (void)cjtag_oscan1_cycle(1U, 1U);    /* Update-DR */
    (void)cjtag_oscan1_cycle(0U, 1U);    /* Idle */

    if (request & DAP_TRANSFER_TIMESTAMP) {
        DAP_Data.timestamp = TIMESTAMP_GET();
    }

    n = DAP_Data.transfer.idle_cycles;
    while (n--) {
        (void)cjtag_oscan1_cycle(0U, 1U); /* Idle */
    }

    return (uint8_t)ack;
}

#endif /* (DAP_JTAG != 0) */
