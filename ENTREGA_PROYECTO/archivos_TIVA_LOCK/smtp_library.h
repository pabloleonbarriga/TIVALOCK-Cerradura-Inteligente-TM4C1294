/*
 * smpt_library.h
 *
 *  Created on: 7 dic. 2020
 *      Author: pablo
 */

#ifndef SMPT_LIBRARY_H_
#define SMPT_LIBRARY_H_


// Definiciones para la configuración del reloj del sistema (ticks)
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

// Definiciones para la prioridad de las interrupciones
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

// Dirección IP
uint32_t g_ui32IPAddress;

// Variable booleana volatil para el led. Parpadea a la frecuencia de SISTICKHZ
volatile bool g_bLED;

void lwIPHostTimerHandler(void);
void SysTickIntHandler(void);
static void close_conn (struct tcp_pcb *pcb);
static err_t echo_recv( void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static tcp_connected_fn connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
void SMTP_start(void);
void config_SMTP(uint32_t RELOJ);

#endif /* SMPT_LIBRARY_H_ */
