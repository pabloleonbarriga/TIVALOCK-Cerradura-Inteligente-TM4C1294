/*
 * smtp_library.c
 *
 *  Created on: 7 dic. 2020
 *      Author: pablo
 */



#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "driverlib2.h"
#include "drivers/pinout.h"
#include "lwip/tcp.h"

#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/ustdlib.h"

#include "smtp_library.h"
#include "variables_globales.h"



// Variable buffer para recibir el mensaje devuelto por el servidor SMTP
char g_pctcpBuffer[4096] = {0};

// Variable que indica que se ha encontrado una IP
bool IP_establecida_una_vez = false;

// Cadena para almacenar la direccion IP
char cadena_IP[16];

// Estado actual de la comunicación SMTP
short int estado_actual = 0;

/*
 * Vector que almacena la secuencia de respuestas que se reciben
 * desde el servidor smtp.gmail.com. Se comprobará que el código
 * recibido después de cada intercambio de mensajes sea el adecuado,
 * y en caso contrario se cerrará la conexión y se reintentará
 * hasta un máximo de N veces mandar el correo electrónico.
 */
short int secuencia_respuestas[] = {220, 250, 555, 250, 334, 334, 235, 250, 250, 354, 250, 221};

// Almacena el código recibido para comparar
short int codigo_recibido = 0;

// Buffer en el que se almacena el código recibido
char buffer_codigo_recibido[3]={0};

// Actua de indice en bucles for
char indice;


// Funcion requerida por la librería lwIP.
void lwIPHostTimerHandler(void)
{
    uint32_t IP_obt_lwIPAddrGet;

    // Obtener IP actual
    IP_obt_lwIPAddrGet = lwIPLocalIPAddrGet();

    // Si la IP obtenita es distinta a la anterior
    if(IP_obt_lwIPAddrGet != g_ui32IPAddress)
    {
        // Si lwIPLocalIPAddrGet() devuelve 0xffff..., aún no hay un link activo
        if(IP_obt_lwIPAddrGet == 0xffffffff)
        {
        }
        // Si lwIPLocalIPAddrGet() devuelve 0x000..., aún no hay direccion IP
        else if(IP_obt_lwIPAddrGet == 0)
        {
        }
        else
        {
            // Permite entrar en el if siguiente
            IP_establecida_una_vez = true;
        }

        // Guardar IP
        g_ui32IPAddress = IP_obt_lwIPAddrGet;
    }

    // Si se obtiene IP, mostrar
    if((IP_obt_lwIPAddrGet != 0) && (IP_obt_lwIPAddrGet != 0xffffffff) && IP_establecida_una_vez == true)
    {
        IP_establecida_una_vez = false;
    }
}

// Función interrupciones de los ticks del sistema
void SysTickIntHandler(void)
{
    // LLamada al temporizadaor del lwIP
    lwIPTimer(SYSTICKMS);

    // Cambiar estado del led
    g_bLED = true;
}

// Devolución de llamada para cerrar la conexión
static void close_conn (struct tcp_pcb *pcb)
{
    // A todas las funciones se les pasa un parámetro nulo, de manera que no recibe o maneja argumentos

    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_close(pcb);
}

/*
 * Defines con aquellos parámetros correspondiente con la
 * comunicación entre cliente (TM4C) y servidor (smtp.gmail.com)
 * del protocolo SMTP que permanecen "constantes". Estos parámetros
 * son constantes en todos los correos. Lo que debe modificarse es el
 * correo electronico del usuario al que se le quiere enviar el código
 * de verificación.
 */
#define SMTP_CMD_EHLO               "HELO GOOGLE"
#define SMTP_CMD_EHLO_LEN           11

#define SMTP_CMD_STARTTLS           "STARTTLS AUTH LOGIN"
#define SMTP_CMD_STARTTLS_LEN       19

#define SMTP_CMD_AUTHLOGIN          "AUTH LOGIN"
#define SMTP_CMD_AUTHLOGIN_LEN      10

#define SMTP_CMD_MAIL_FROM          "MAIL FROM: <"
#define SMTP_CMD_MAIL_FROM_LEN      12

#define SMTP_CMD_RCPT_TO            "RCPT TO: <"
#define SMTP_CMD_RCPT_TO_LEN        10

#define SMTP_CMD_DATA               "DATA"
#define SMTP_CMD_DATA_LEN           4

#define SMTP_CMD_FROM               "From: <"
#define SMTP_CMD_FROM_LEN           7

#define SMTP_CMD_TO                 "To: <"
#define SMTP_CMD_TO_LEN             5

#define SMTP_CMD_SUBJECT            "Subject: TIVA LOCK - Código de verificación"
#define SMTP_CMD_SUBJECT_LEN        43
#define SMTP_CMD_GENERIC_END        ">\r\n"
#define SMTP_CMD_GENERIC_END_LEN    3


#define SMTP_CMD_BODY_FINISHED      "\r\n.\r\n"
#define SMTP_CMD_BODY_FINISHED_LEN  5

#define SMTP_CMD_QUIT               "QUIT"
#define SMTP_CMD_QUIT_LEN           4

#define SMTP_USER                   "c21hcnQubG9jay50bTRjQGdtYWlsLmNvbQ=="
#define SMTP_USER_LEN               36
#define SMTP_PASS                   "dG00Yy5zbTRydC5sMGNr"
#define SMTP_PASS_LEN               20
#define SMTP_EMAIL_FROM             "smart.lock.tm4c@gmail.com"
#define SMTP_EMAIL_FROM_LEN         25
#define SMTP_EMAIL_TO               "thisisnotspamatalltrustme@gmail.com"
#define SMTP_EMAIL_TO_LEN           35

#define SMTP_TEST              "MENSAJE DESDE TIVA TM4C4912"
#define SMTP_TEST_LEN          27

#define SMTP_CRLF               "\r\n"
#define SMTP_CRLF_LEN           2

// 0 si no se envía, 1 si se envía
int correo_exito = 2;

// Lleva cuenta de reintentos
int reintentos = 0;

int long_mensaje = 0;
// Funcion que maneja la información devuelta por el host
static err_t comunicacion_SMTP( void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{

    int ind_rand = 0;
    char *data_rcvd;    // Caracter en el que se almacena el puntero que apunta a la posicion de memoria de datos recibidos
    char cuerpo_mensaje[] = "\n\rAqui tiene el codigo de verificacion para TIVA LOCK \n\rSi no lo ha solicitado contacte con el administrador.";
    int long_cuerpo = 0;
    // Si no hay errores y p es no null
    if (err == ERR_OK && p != NULL)
    {

        // Llamada tcp_recved cuando se han procesado los datos
        tcp_recved(pcb, p->tot_len);

        // Almacenar el puntero a la información recibida
        data_rcvd = (char *)p->payload;



        // Se almacena en el buffer el código recibido
        for (indice=0;indice<3;indice++){
            buffer_codigo_recibido[indice] = *(data_rcvd + indice);
        }

        // Convertir cadena con código a short int
        codigo_recibido = atoi(buffer_codigo_recibido);

        /*
         * Antes de entrar en el switch-case se comprueba que el código
         * de confirmación recibido se corresponde con el que almacenado
         * en el vector de respuestas. En caso contrario, se cerrará la
         * conexión, y se aumentará el contador de intentos.
         */
        if (codigo_recibido != secuencia_respuestas[estado_actual]){
            estado_actual = 0;
            correo_exito = 0;
            reintentos++;
            close_conn(pcb);    // Cerrar la conexión
        }
        else{
            switch (estado_actual){

            // HELO GOOGLE
            case 0:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_EHLO, SMTP_CMD_EHLO_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // STARTTLS
            case 1:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_STARTTLS, SMTP_CMD_STARTTLS_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // HELO GOOGLE
            case 2:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_EHLO, SMTP_CMD_EHLO_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // AUTH LOGIN
            case 3:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_AUTHLOGIN, SMTP_CMD_AUTHLOGIN_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // USUARIO, BASE 64
            case 4:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_USER, SMTP_USER_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // CONTRASENA, BASE 64
            case 5:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_PASS, SMTP_PASS_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // MAIL FROM:
            case 6:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_MAIL_FROM, SMTP_CMD_MAIL_FROM_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_EMAIL_FROM, SMTP_EMAIL_FROM_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CMD_GENERIC_END, SMTP_CMD_GENERIC_END_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // RCPT TO:
            case 7:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_RCPT_TO, SMTP_CMD_RCPT_TO_LEN, TCP_WRITE_FLAG_COPY);
                // AQUÍ SE METE LA DIRECCIÓN DE CORREO A LA QUE SE QUIERE ENVIAR EL CÓDIGO
                // tcp_write(pcb, DIRECCION DE CORREO, LONGITUD DIRECCION DE CORREO, 0);
                //                tcp_write(pcb,SMTP_EMAIL_FROM, SMTP_EMAIL_FROM_LEN, TCP_WRITE_FLAG_COPY);
                //                char hola[100] = "pablo5leon5@gmail.com";
                long_mensaje = 0;
                while (correorecuperado[long_mensaje]!= 0)
                    long_mensaje++;
                // cuerpo del mensaje
                tcp_write(pcb,&correorecuperado[0], long_mensaje, TCP_WRITE_FLAG_COPY);



                tcp_write(pcb,SMTP_CMD_GENERIC_END, SMTP_CMD_GENERIC_END_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // DATA
            case 8:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_DATA, SMTP_CMD_DATA_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // INFORMACIÓN DEL MENSAJE
            case 9:{

                // from
                tcp_write(pcb,SMTP_CMD_FROM, SMTP_CMD_FROM_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_EMAIL_FROM, SMTP_EMAIL_FROM_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CMD_GENERIC_END, SMTP_CMD_GENERIC_END_LEN, TCP_WRITE_FLAG_COPY);

                // to
                tcp_write(pcb,SMTP_CMD_TO, SMTP_CMD_TO_LEN, TCP_WRITE_FLAG_COPY);
                // AQUÍ SE METE LA DIRECCIÓN DE CORREO A LA QUE SE QUIERE ENVIAR EL CÓDIGO
                // tcp_write(pcb, DIRECCION DE CORREO, LONGITUD DIRECCION DE CORREO, 0);
                tcp_write(pcb,SMTP_EMAIL_FROM, SMTP_EMAIL_FROM_LEN,TCP_WRITE_FLAG_COPY);


                tcp_write(pcb,SMTP_CMD_GENERIC_END, SMTP_CMD_GENERIC_END_LEN, TCP_WRITE_FLAG_COPY);

                // subject
                tcp_write(pcb,SMTP_CMD_SUBJECT, SMTP_CMD_SUBJECT_LEN, TCP_WRITE_FLAG_COPY);
                // AQUÍ SE METE EL SUJETO DEL CORREO ELECTRONICO
                // tcp_write(pcb, SUBJECT, LONGITUD SUBJECT, 0);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);

                // enter
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);

                // cuerpo del mensaje
                for (ind_rand=0;ind_rand<long_clave+1;ind_rand++)
                    rand_number[ind_rand] = 0;
                long_mensaje = 0;
                srand(aleatoriedad);
                while (long_mensaje<long_clave+1){
                    if(long_mensaje<long_clave){
                        //                        while(rand_number[long_mensaje]<48 || rand_number[long_mensaje]>90)
                        while(rand_number[long_mensaje]<'0' || (rand_number[long_mensaje]>'9' && rand_number[long_mensaje]<'A') || (rand_number[long_mensaje]>'Z') )
                            rand_number[long_mensaje] = rand() % 256;
                    }
                    else
                        rand_number[long_mensaje] = 0;
                    long_mensaje++;
                }
                // cuerpo del mensaje
                while (cuerpo_mensaje[long_cuerpo]!= 0)
                    long_cuerpo++;
                tcp_write(pcb,&rand_number[0], long_mensaje , TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,&cuerpo_mensaje[0], long_cuerpo , TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);
                // finaliza mensaje con enter . enter
                tcp_write(pcb,SMTP_CMD_BODY_FINISHED, SMTP_CMD_BODY_FINISHED_LEN, TCP_WRITE_FLAG_COPY);
                break;
            }

            // QUIT
            case 10:{
                // Se rellena el buffer de salida con el mensaje
                tcp_write(pcb,SMTP_CMD_QUIT, SMTP_CMD_QUIT_LEN, TCP_WRITE_FLAG_COPY);
                tcp_write(pcb,SMTP_CRLF, SMTP_CRLF_LEN, TCP_WRITE_FLAG_COPY);

                correo_exito = 1;
                break;
            }
            }
            // Se obliga a mandar el mensaje
            tcp_output(pcb);

            // Aumentar el estado actual
            estado_actual++;
        }


        // Liberar y "dereferenciar" aquellas cadenas que hayan quedado residuales
        pbuf_free(p);
    }
    else
    {
        // Si se produce un error, liberar el buffer
        pbuf_free(p);
    }

    // Si el host remoto solicita cerrar la conexión
    if(err == ERR_OK && p == NULL)
    {
        close_conn(pcb);
    }
    return ERR_OK;
}


// Función llamada una vez se establece conexión, prepara el estado del intercambio
static tcp_connected_fn connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err){
    // Colocar el estado actual al comienzo del intercambio de mensajes entre el host y el cliente
    estado_actual = 0;
    return ERR_OK;
}

// Funcion que inicializa la conexión para el SMTP
void SMTP_start(void)
{
    // Crear una estructura de tipo tcp_pcb
    struct tcp_pcb *tcp_pcb;

    // Nuevo identificador de conexión TCP
    tcp_pcb = tcp_new();

    // Cualquier dirección, puerto 587 (TLS gmail)
    tcp_bind(tcp_pcb, IP_ADDR_ANY, 587);

    // Variable en la que se almacena una de las muchas direcciones IP de los servidores SMTP de GMAIL
    struct ip_addr IP_smtp;
    IP4_ADDR(&IP_smtp,74,125,133,109);

    // Mientras no se tenga direccion IP
    while ((g_ui32IPAddress == 0) || (g_ui32IPAddress == 0xffffffff)){
        SysCtlDelay(40000*10);
    }

    // Conectarse al servidor SMTP de GMAIL, llamar a connected_callback
    tcp_connect(tcp_pcb, &IP_smtp, 587, (tcp_connected_fn) connected_callback);


    // Indicar qué función será llamada cuando se reciban datos del servidor
    tcp_recv(tcp_pcb, comunicacion_SMTP);
}

void config_SMTP(uint32_t RELOJ){
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];


    //
    // Make sure the main oscillator is enabled because this is required by
    // the PHY.  The system must have a 25MHz crystal attached to the OSC
    // pins. The SYSCTL_MOSC_HIGHFREQ parameter is used when the crystal
    // frequency is 10MHz or higher.
    //
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);


    // Configurar los pines ETHERNET del microcontrolador
    PinoutSet(true, false); // Mandando true/false se activan los pines del ETHERNET y no lo del bus USB


    // Configurar interrupciones de ticks del sistema
    SysTickPeriodSet(RELOJ / SYSTICKHZ);
    SysTickEnable();
    SysTickIntEnable();
    //
    // Configure the hardware MAC address for Ethernet Controller filtering of
    // incoming packets.  The MAC address will be stored in the non-volatile
    // USER0 and USER1 registers.
    //
    FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        //
        // We should never get here.  This is an error if the MAC address has
        // not been programmed into the device.  Exit the program.
        // Let the user know there is no MAC address.
        //
        while(1)
        {
        }
    }

    //
    // Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
    // address needed to program the hardware registers, then program the MAC
    // address into the Ethernet Controller registers.
    //
    pui8MACArray[0] = ((ui32User0 >>  0) & 0xff);
    pui8MACArray[1] = ((ui32User0 >>  8) & 0xff);
    pui8MACArray[2] = ((ui32User0 >> 16) & 0xff);
    pui8MACArray[3] = ((ui32User1 >>  0) & 0xff);
    pui8MACArray[4] = ((ui32User1 >>  8) & 0xff);
    pui8MACArray[5] = ((ui32User1 >> 16) & 0xff);

    //
    // Initialize the lwIP library, using DHCP.
    //
    lwIPInit(RELOJ, pui8MACArray, 0, 0, 0, IPADDR_USE_DHCP);

    //    SMTP_start();

    // Establecer prioridad de interrupciones. TICKS>EMAC
    IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);

}
