#include "JTAG_DP.h"
#include "DAP.h"
#include "setting.h"

static uint8_t JTAG_IsCJTAG(void)
{
    return HSLink_Setting.jtag_protocol == JTAG_PROTOCOL_CJTAG ? 1U : 0U;
}

uint8_t JTAG_ProtocolReady(void)
{
    return (HSLink_Setting.jtag_protocol == JTAG_PROTOCOL_JTAG) ||
           (HSLink_Setting.jtag_protocol == JTAG_PROTOCOL_CJTAG);
}

void PORT_JTAG_SETUP (void)
{
    if (JTAG_ProtocolReady() == 0U) {
        return;
    }

    if (JTAG_IsCJTAG()) {
        CJTAG_PORT_JTAG_SETUP();
        return;
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI) {
        SPI_PORT_JTAG_SETUP();
    } else {
        IO_PORT_JTAG_SETUP();
    }
}

void JTAG_Sequence (uint32_t info, const uint8_t *tdi, uint8_t *tdo)
{
    if (JTAG_ProtocolReady() == 0U) {
        return;
    }

    if (JTAG_IsCJTAG()) {
        CJTAG_JTAG_Sequence(info, tdi, tdo);
        return;
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI) {
        SPI_JTAG_Sequence(info, tdi, tdo);
    } else {
        IO_JTAG_Sequence(info, tdi, tdo);
    }
}

uint32_t JTAG_ReadIDCode (void)
{
    if (JTAG_ProtocolReady() == 0U) {
        return 0U;
    }

    if (JTAG_IsCJTAG()) {
        return CJTAG_JTAG_ReadIDCode();
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI) {
        return SPI_JTAG_ReadIDCode();
    } else {
        return IO_JTAG_ReadIDCode();
    }
}

void JTAG_WriteAbort (uint32_t data)
{
    if (JTAG_ProtocolReady() == 0U) {
        return;
    }

    if (JTAG_IsCJTAG()) {
        CJTAG_JTAG_WriteAbort(data);
        return;
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI) {
        SPI_JTAG_WriteAbort(data);
    } else {
        IO_JTAG_WriteAbort(data);
    }
}

void JTAG_IR (uint32_t ir)
{
    if (JTAG_ProtocolReady() == 0U) {
        return;
    }

    if (JTAG_IsCJTAG()) {
        CJTAG_JTAG_IR(ir);
        return;
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI) {
        SPI_JTAG_IR(ir);
    } else {
        IO_JTAG_IR(ir);
    }
}

uint8_t JTAG_Transfer(uint32_t request, uint32_t *data)
{
    if (JTAG_ProtocolReady() == 0U) {
        return DAP_TRANSFER_ERROR;
    }

    if (JTAG_IsCJTAG()) {
        return CJTAG_JTAG_Transfer(request, data);
    }

    if (HSLink_Setting.jtag_port_mode == PORT_MODE_SPI)
    {
        return SPI_JTAG_Transfer(request, data);
    }
    else
    {
        return IO_JTAG_Transfer(request, data);
    }
}
