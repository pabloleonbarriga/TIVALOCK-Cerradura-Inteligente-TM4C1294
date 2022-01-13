/*
 * variables_globales.h
 *
 *  Created on: 7 dic. 2020
 *      Author: pablo
 */

#ifndef VARIABLES_GLOBALES_H_
#define VARIABLES_GLOBALES_H_

extern char g_pctcpBuffer[4096];
extern bool IP_establecida_una_vez;
extern char cadena_IP[16];
extern short int estado_actual;
extern unsigned char correo[];
extern uint32_t correonum[100];
extern unsigned char correorecuperado[256];
extern uint32_t test;
#define long_clave 5
extern char rand_number[long_clave+1];
extern unsigned long aleatoriedad;
extern int correo_exito;
extern int reintentos;
//extern
//extern



#endif /* VARIABLES_GLOBALES_H_ */
