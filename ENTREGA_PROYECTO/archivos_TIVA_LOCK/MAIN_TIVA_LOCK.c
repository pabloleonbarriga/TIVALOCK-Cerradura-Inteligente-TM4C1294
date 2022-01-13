
// =======================================================================
//                              TIVA LOCK
// =======================================================================
//                             Creado por:
//                          Pablo León Barriga
//                         Javier Moreno Prieto
// =======================================================================

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
#include "FT800_TIVA.h"
#include "Mfrc522.h"

// =======================================================================
// Variable Declarations
// =======================================================================

char chipid = 0;                        // Holds value of Chip ID read from the FT800

unsigned long cmdBufferRd = 0x00000000;         // Store the value read from the REG_CMD_READ register
unsigned long cmdBufferWr = 0x00000000;         // Store the value read from the REG_CMD_WRITE register
unsigned int t=0;

// ############################################################################################################
// User Application - Initialization of MCU / FT800 / Display
// ############################################################################################################

int Fin_Rx=0;
char Buffer_Rx;
unsigned long POSX, POSY, BufferXY;
unsigned long POSYANT=0;
unsigned int CMD_Offset = 0;
unsigned long REG_TT[6];
const int32_t REG_CAL[6]={21696,-78,-614558,498,-17021,15755638};
const int32_t REG_CAL5[6]={32146, -1428, -331110, -40, -18930, 18321010};

#define NUM_SSI_DATA            3


// ############################################################################################################
// Variables / funciones nuestras
// ############################################################################################################

// Variables para el RELOJ
int RELOJ;

// Un milisegundo con el RELOJ de 120MHz (40000*3)
#define MSEC 40000


// Funciones de configuración de periféricos, interrupciones y otros módulos utilizados
void config_RC522();
void config_PWM();




// Funciones relacionadas con la lectura y funcionamiento del RFID
void RFID();

// Funciones relacionadas con la lectura/escritura de la memoria Flash
int EncontrarEspacio(int posicion);
void GuardarADMIN(int posicion);
void GuardarUSUARIO(int posicion, unsigned char* ID_usuario, char* correo_usuario, char* nombre_usuario);
int ComprobarID(int posicion);
void ObtenerCorreo();
void ObtenerNombre();
void BorrarBufferCorreo();
void BorrarBufferNombre();
void BorrarBufferClave();
void BorrarBufferEscritura();
void BorrarBufferEscrituraCorreo();
void BorrarBufferEscrituraNombre();

// Funciones relacionadas con el temporizador
void config_timer1(int Reloj);
void interrupcion_timer1(void);
void EsperaSeg( int segundos);


// Variables PWM
volatile int Max_pos = 4400; //3750
volatile int Min_pos = 1300; //1875
int PeriodoPWM;



// Otras variables
int tarjeta_detectada=0;
char TXT_1[100];
bool clave_correcta;
unsigned long aleatoriedad=0;
int contador_intentos=0;
int resp = 0;
int dib;
int tiempo = 0;
int estado=0;
int numero_usuario = 0; // contador de que usuario se ha registrado
bool bloqueo = 0;

//----------------------------------------------------------------------------------------------\\
//---------------DEFINES / VARIABLES PARA LA PARTE DE CORREO EN FLASH / SMTP--------------------\\
//----------------------------------------------------------------------------------------------\\

// Saltos en bytes para almacenar en memoria
#define SaltoID 8
#define SaltoCorreo 256
#define SaltoUsuarios 512
#define InicioMemoria 0x30000


// https://tools.ietf.org/html/rfc5321#section-4.5.3
// Tamaño maximo que puede tener una direccion de correo electrónico según RFC 5321
#define MAX_long_correo 256
char buffer_correo[MAX_long_correo] = {0};

// Buffer para el nombre de usuario
#define MAX_long_nombre 200
char buffer_nombre[MAX_long_nombre] = {0};

// Buffer clave escrita
char rand_number[long_clave+1] = {0};
char buffer_clave[long_clave] = {0};


// Variables para distintos bucles for
int i, j;

// Variables lectura / escritura en flash
uint32_t byte_leido_flash;
unsigned char correorecuperado[MAX_long_correo] = {0}; // para comprobar la lectura de la flash
unsigned char nombrerecuperado[MAX_long_nombre] = {0}; // para comprobar la lectura de la flash
uint32_t byte_envio;
int long_correo = 0;
int posicion = InicioMemoria;
int posicion_libre;
int longitud_correo_rec = 0;
int ok; // para mantenernos en el while

// Utilidades RFID
int chipSelectPin = 0x2;    //PQ1
int NRSTPD = 0x10;    //PB4

#define CARD_LENGTH 5
uint8_t status;
unsigned char str[MAX_LEN];
unsigned char cardREAD[100];
unsigned char cardID[CARD_LENGTH];

//----------------------------------------------------------------------------------------------\\
//----------------------DEFINES / VARIABLES PARA LA PARTE DE TECLADO----------------------------\\
//----------------------------------------------------------------------------------------------\\

// Dibuja una pantalla inicial, espera a que el usuario toque la pantalla
void pantalla_inicial(void);

// Funciones relacionadas con el teclado normal en pantalla
void pantalla(short int campo);
void get_teclado(short int campo);
void pantalla_teclado(short int campo);

// Indice usado en bucles dentro de las funciones del teclado
char indice_teclado = 0;

// Variables que almacenan la tecla actual y la anterior
char tecla = 0;
char tecla_p = 0;

// Esquina superior izquierda de la primera tecla, primera fila
short int fila1_inicio_x = 10;    // Coordenada x, esquina superior izquierda
short int fila1_inicio_y;// = 120;    // Coordenada y, esquina superior izquierda

// Altura y anchura de las letras
short int tecla_w;   // Anchura, se calcula más adelante
short int tecla_h = 25;   // Altura

// Anchura rectángulo filas 1, 2 y 3
short int fila1_rect_w;
short int fila2_rect_w;
short int fila3_rect_w;

// Fila 0, de los números, añadida posteriormente


// Separacion entre teclado y borde pantalla
short int tecla_borde;  // pix

// Separacion vertical entre teclas
short int tecla_separacion = 8;

// Definir cadenas de cada fila del teclado, sin la ñ
char *fila0 = "1234567890";

char *fila1_minus = "qwertyuiop";
char *fila1_mayus = "QWERTYUIOP";
char *fila1_especiales = "!$%&/()=?-";

char *fila2_minus = "asdfghjkl";
char *fila2_mayus = "ASDFGHJKL";
char *fila2_especiales = "|@#'^*,.;";

char *fila3_minus = "zxcvbnm";
char *fila3_mayus = "ZXCVBNM";
char *fila3_especiales = "[]{}<>_";

char *fila4 = "@     .";



#define n_tecla_fila_1 10
#define n_tecla_fila_2 9
#define n_tecla_fila_3 7
#define n_filas 5


// Calculo comienzo teclas siguientes filas, dependen de fila1
short int fila0_inicio_y;

short int fila2_inicio_x;
short int fila2_inicio_y;

short int fila3_inicio_x;
short int fila3_inicio_y;

short int fila4_inicio_y;

// Variables para los botones de shift, Options, Delete y Enter
short int shift_boton = 0;
short int shift_boton_p = 0;
bool SHIFT = false;

short int enter_boton = 0;
short int enter_boton_p = 0;
bool ENTER = false;

short int options_boton = 0;
short int options_boton_p = 0;
bool OPTIONS = false;

short int delete_boton = 0;
short int delete_boton_p = 0;
bool DELETE = false;

short int space_boton = 0;
short int space_boton_p = 0;
bool SPACE = false;

//----------------------------------------------------------------------------------------------\\
//----------------------DEFINES / VARIABLES PARA LA PARTE DE BITMAPS----------------------------\\
//----------------------------------------------------------------------------------------------\\

// Solo vamos a tener 2 BMP como maximo a la  vez
#define ram_addr_BMP1 0x0000
#define ram_addr_BMP2 0x6000

// Datos del tamano en bytes y de la posición de la flash en la que está almacenado cada BMP

// LOGO
#define nbytes_logo 2512
#define flash_addr_logo 0x7D000

// Ancho y alto en pixeles de la imagen
#define ancho_pix_logo 200
#define alto_pix_logo  200

// NFC
#define nbytes_nfc 2347
#define flash_addr_nfc 0x7D9D0

// Ancho y alto en pixeles de la imagen
#define ancho_pix_nfc 200
#define alto_pix_nfc  192

// RECONOCIDO
#define nbytes_rec 3394
#define flash_addr_rec 0x7E2FB

// Ancho y alto en pixeles de la imagen
#define ancho_pix_rec 200
#define alto_pix_rec  200

// NO RECONOCIDO
#define nbytes_norec 3357
#define flash_addr_norec 0x7F03D

// Ancho y alto en pixeles de la imagen
#define ancho_pix_norec 200
#define alto_pix_norec  200

// ok
#define nbytes_ok 543
#define flash_addr_ok 0x7FD5A

// Ancho y alto en pixeles de la imagen
#define ancho_pix_ok 80
#define alto_pix_ok  80

// err
#define nbytes_err 783
#define flash_addr_err 523983

// Ancho y alto en pixeles de la imagen
#define ancho_pix_err 80
#define alto_pix_err  80

// lock
#define nbytes_lock 2850
#define flash_addr_lock 524501

// Ancho y alto en pixeles de la imagen
#define ancho_pix_lock 200
#define alto_pix_lock 200

// unlock
#define nbytes_unlock 2805
#define flash_addr_unlock 527351

// Ancho y alto en pixeles de la imagen
#define ancho_pix_unlock 200
#define alto_pix_unlock 200


// Comprueba si se ha pulsado aceptar o cancelar en la comprobacion de correo/usuario
int comp_boton(){
    // Variable a devolver en funcion de lo pulsado
    int respuesta=0;
    while(respuesta == 0)
    {
        Lee_pantalla();
        if ((POSX > (HSIZE/2-ancho_pix_ok/2 - HSIZE/4) && POSX < (HSIZE/2 + ancho_pix_ok/2 - HSIZE/4))&& (POSY > VSIZE*3/4-alto_pix_ok/2 && POSY<VSIZE*3/4+alto_pix_ok/2)){
            respuesta=1;
        }
        if ((POSX > HSIZE/2-ancho_pix_err/2 + HSIZE/4 && POSX < (HSIZE/2 + ancho_pix_err/2 + HSIZE/4))&& (POSY > VSIZE*3/4-alto_pix_err/2 && POSY<VSIZE*3/4+alto_pix_err/2)){
            respuesta=2;
        }
    }
    return respuesta;
}

// Interrupcion del timer cada 50ms
void interrupcion_timer1(){
    // Variable para contar
    tiempo++;
    // Variable para semilla de generacion del codigo aleatorio
    aleatoriedad++;
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Borra la interrupción del temporizador
    SysCtlDelay(100);
}

int main(void)
{
    // Configuración del RELOJ a 120 MHz
    RELOJ=SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    // Configuracion reloj
    config_timer1(RELOJ);

    // Configuración del PWM
    config_PWM();

    // Configuración e inicialización del lector RC522
    config_RC522();
    RFID_init();

    // Configuración del cliente SMTP
    config_SMTP(RELOJ);

    // Boosterpack 1 para FT800, pintar pantalla inicial
    HAL_Init_SPI(1, RELOJ);
    pantalla_inicial();

    // Guardamos datos del admin
    GuardarADMIN(posicion);

    // Determina los segundos a esperar en los distintos estados
    int esperar=0;

    while(1){
        // Lee posicion del dedo de la pantalla
        Lee_pantalla();

        // Pantalla en blanco y configuracion de colores
        Nueva_pantalla(0xff,0xff,0xff);
        ComColor(0,0,0);
        ComFgcolor(45, 150, 100);

        // Maquina de estados
        switch(estado){
        // Estado de reposo, espera a lectura de tarjeta RFID
        case 0:
            dib = 0;
            // Cargar imagen RFID y dibujar
            almacenar_imagen_externa(flash_addr_nfc, ram_addr_BMP1, nbytes_nfc);
            Nueva_pantalla(0xff,0xff,0xff);
            dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_nfc, alto_pix_nfc, HSIZE/2-ancho_pix_nfc/2, VSIZE/2-alto_pix_nfc/2, 255, 0);
            // Comprobacion de bloqueo de cerradura
            if (bloqueo == 1)
            {
                ComColor(255,0,0);
                ComTXT(HSIZE/2,VSIZE*7/8, 28, OPT_CENTER,"Cerradura bloqueada"); //pinta un texto
            }
            Dibuja();
            // bucle hasta que se lea tarjeta
            while(tarjeta_detectada!=1){
                SysCtlSleep();
                RFID();
            }
            estado=1;
            tarjeta_detectada=0;
            break;
            // Identificamos al usuario
        case 1:
            dib = 1;
            // Posicion de inicio de almacenamiento de usuarios
            posicion=InicioMemoria;
            // Comprobacion de usuario registrado
            ok=ComprobarID(posicion);
            // Si el usuario es reconocido
            if (ok==2)
            {
                // Cargar imagen TARJETA RECONOCIDA y dibujar
                almacenar_imagen_externa(flash_addr_rec, ram_addr_BMP1, nbytes_rec);
                Nueva_pantalla(0xff,0xff,0xff);
                dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_rec, alto_pix_rec, HSIZE/2-ancho_pix_rec/2, VSIZE/2-alto_pix_rec/2, 255, 0);
                // Si la tarjeta es de admin
                if ( numero_usuario == 0 )
                    estado = 2;
                // Usuario normal
                else
                {
                    // Comprobacion de bloqueo de cerradura
                    if (bloqueo == 1)
                    {
                        ComColor(255,0,0);
                        ComTXT(HSIZE/2,VSIZE*7/8, 28, OPT_CENTER,"Cerradura bloqueada"); //pinta un texto
                        estado = 0;
                    }
                    else
                        estado = 14;
                }
                // Sonido de okey
                TocaNota(0x41,70-24);
                SysCtlDelay(10*MSEC);
                TocaNota(0x41,73-24);
            }
            // Si el usuario no esta registrado
            else
            {
                // Cargar imagen TARJETA NO RECONOCIDA y dibujar
                almacenar_imagen_externa(flash_addr_norec, ram_addr_BMP1, nbytes_norec);
                Nueva_pantalla(0xff,0xff,0xff);
                dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_norec, alto_pix_norec, HSIZE/2-ancho_pix_norec/2, VSIZE/2-alto_pix_norec/2, 255, 0);
                estado = 0;
                // Sonido de error
                TocaNota(0x42,70-24);
                SysCtlDelay(10*MSEC);
                TocaNota(0x42,67-24);
            }
            // Espera de 1 segundo
            esperar=1;
            tarjeta_detectada = 0;
            break;
            // Pantalla bienvenida admin
        case 2:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Bienvenido admin");
            ComTXT(HSIZE/2,VSIZE*5/8, 26, OPT_CENTER,"Se esta enviando el codigo de verificacion");
            // Se envia correo de verificacion
            estado=15;
            // Espera de 1 segundo
            esperar=1;
            break;
            // Menu admin
        case 3:
            dib = 1;
            ComColor(255,255,255);
            // Botones
            if(BotonFlat(HSIZE*1/6, VSIZE*2/24, HSIZE*4/6, VSIZE*1/6, 28, "Agregar usuario"))
                estado=4;
            if(BotonFlat(HSIZE*1/6, VSIZE*7/24, HSIZE*4/6, VSIZE*1/6, 28, "Abrir cerradura"))
                estado=19;
            if(BotonFlat(HSIZE*1/6, VSIZE*12/24, HSIZE*4/6, VSIZE*1/6, 28, "Volver"))
                estado=21;
            // Opcion de desbloquear en caso de bloqueo de cerradura
            if (bloqueo == 1)
            {
                ComFgcolor(255, 0, 0);
                if(BotonFlat(HSIZE*1/6, VSIZE*17/24, HSIZE*4/6, VSIZE*1/6, 28, "Desbloquear"))
                {
                    bloqueo = 0;
                    estado=21;
                }
            }
            break;
            // Agregar ID usuario
        case 4:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Acerque la tarjeta del nuevo usuario"); //pinta un texto
            // Lectura de tarjeta
            RFID();
            // Cuando se lea la  tarjeta
            if (tarjeta_detectada==1)
            {
                // Posicion de inicio de almacenamiento de usuarios
                posicion=InicioMemoria;
                // Comprobamos que esa tarjeta no se haya usado aun
                ok=ComprobarID(posicion);
                // Tarjeta todavia no registrada, la guardamos
                if (ok == 0)
                {
                    estado=5;
                    tarjeta_detectada=0;
                    TocaNota(0x41,70-24);
                    SysCtlDelay(10*MSEC);
                    TocaNota(0x41,73-24);
                }
                // Tarjeta ya usada
                else
                {
                    estado=6;
                    tarjeta_detectada=0;
                    TocaNota(0x42,70-24);
                    SysCtlDelay(10*MSEC);
                    TocaNota(0x42,67-24);
                }
            }
            break;
            // Mensaje de introduccion de correo
        case 5:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Introduzca el correo del nuevo usuario");
            // Espera de 1 segundo
            esperar = 1;
            estado = 7;
            break;
            // Mensaje de tarjeta ya usada
        case 6:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Tarjeta ya registrada"); //pinta un texto
            // Espera de 1 segundo
            esperar = 1;
            estado = 4;
            break;
            // Escribir correo usuario
        case 7:
            dib = 0;
            // Funcion para mostrar teclado
            pantalla_teclado(0);
            estado=8;
            break;
            // Confirmacion de correo
        case 8:
            dib = 0;
            // Cargar imagenes ACEPTAR y CANCELAR y dibujar
            almacenar_imagen_externa(flash_addr_ok, ram_addr_BMP1, nbytes_ok);
            almacenar_imagen_externa(flash_addr_err, ram_addr_BMP2, nbytes_err);
            Nueva_pantalla(0xff,0xff,0xff);
            dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_ok, alto_pix_ok, HSIZE/2-ancho_pix_ok/2 - HSIZE/4, VSIZE*3/4-alto_pix_ok/2, 255, 0);
            dibujar_imagen_externa(ram_addr_BMP2, ancho_pix_err, alto_pix_err, HSIZE/2-ancho_pix_ok/2 + HSIZE/4, VSIZE*3/4-alto_pix_err/2, 255, 0);
            ComColor(0,0,0);
            ComTXT(HSIZE/2,VSIZE*1/3, 28, OPT_CENTER,"Es correcta esta direccion?");
            sprintf(TXT_1, "%s", buffer_correo);
            ComTXT(HSIZE/2,VSIZE/2, 26, OPT_CENTER,TXT_1);
            Dibuja();
            // Funcion para comprobar que se ha pulsado
            resp = comp_boton();
            // Ok
            if ( resp == 1)
            {
                estado=9;
                break;
            }
            // Volver a escribir
            else if (resp == 2)
            {
                // Borrar lo que se ha escrito
                BorrarBufferEscrituraCorreo();
                // Espera de 1 segundo
                esperar=1;
                estado = 7;
                break;
            }

            break;
            // Mensaje introducir nombre de usuario
        case 9:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Introduzca el nombre del nuevo usuario");
            // Espera de 1 segundo
            esperar = 1;
            estado = 10;
            break;
            // Escribir nombre usuario
        case 10:
            dib = 0;
            // Funcion para mostrar teclado
            pantalla_teclado(1);
            estado = 11;
            break;
            // Confirmacion de usuario
        case 11:
            dib = 0;
            // Cargar imagenes ACEPTAR y CANCELAR y dibujar
            almacenar_imagen_externa(flash_addr_ok, ram_addr_BMP1, nbytes_ok);
            almacenar_imagen_externa(flash_addr_err, ram_addr_BMP2, nbytes_err);
            Nueva_pantalla(0xff,0xff,0xff);
            dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_ok, alto_pix_ok, HSIZE/2-ancho_pix_ok/2 - HSIZE/4, VSIZE*3/4-alto_pix_ok/2, 255, 0);
            dibujar_imagen_externa(ram_addr_BMP2, ancho_pix_err, alto_pix_err, HSIZE/2-ancho_pix_ok/2 + HSIZE/4, VSIZE*3/4-alto_pix_err/2, 255, 0);
            ComColor(0,0,0);
            ComTXT(HSIZE/2,VSIZE*1/3, 28, OPT_CENTER,"Es correcto este nombre?");
            sprintf(TXT_1, "%s", buffer_nombre);
            ComTXT(HSIZE/2,VSIZE/2, 26, OPT_CENTER,TXT_1);
            Dibuja();
            // Funcion para comprobar que se ha pulsado
            resp = comp_boton();
            // Ok
            if ( resp == 1)
            {
                estado = 12;
                break;
            }
            // Volver a escribir
            else if (resp == 2)
            {
                // Borrar lo que se ha escrito
                BorrarBufferEscrituraNombre();
                // Espera de 1 segundo
                esperar = 1;
                estado = 10;
                break;
            }
            break;
            // Almacenamos ID, correo y nombre
        case 12:
            dib = 0;
            posicion=InicioMemoria;
            // Busca una posicion de memoria libre para almacenar el usuario
            posicion_libre = EncontrarEspacio(posicion);
            // Funcion para guardar el usuario
            GuardarUSUARIO(posicion_libre, cardID, &buffer_correo[0], &buffer_nombre[0]);
            // Se borran buffers para la proxima
            BorrarBufferEscritura();
            estado = 13;
            break;
            // Mensaje de usuario guardado
        case 13:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Usuario guardado");
            // Espera de 1 segundo
            esperar = 1;
            estado = 21;
            break;
            // Pantalla de bienvenida de usuario
        case 14:
            dib = 1;
            // Funcion para obtener el nombre del usuario
            ObtenerNombre();
            // Mensaje de bienvenida
            sprintf(TXT_1, "Bienvenido %s", nombrerecuperado);
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,TXT_1);
            ComTXT(HSIZE/2,VSIZE*5/8, 26, OPT_CENTER, "Se esta enviando el codigo de verificacion");
            // Espera de 1 segundo
            esperar=1;
            estado=15;
            reintentos = 0;
            break;
            // Envio de codigo de confirmacion al correo
        case 15:
            dib = 0;
            // Leemos el correo del usuario
            ObtenerCorreo();
            // Reiniciamos el contador de intentos (max 3)
            contador_intentos = 0;
            // Obtenemos la longitud del correo ( necesario para el envio )
            longitud_correo_rec = sizeof(correorecuperado);
            // Funcion encargada del envio
            correo_exito = 2;
            SMTP_start();
            while(correo_exito == 2);
            if (correo_exito == 1)
                estado = 16;
            else if (correo_exito == 0 && reintentos < 2)
                estado = 15;
            else
                estado = 22;
            break;
            // Mensaje de email
        case 16:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Introduzca la clave enviada a su correo");
            // Espera de 1 segundo
            esperar = 1;
            estado = 17;
            break;
            // Introduccion de correo recibido
        case 17:
            dib = 0;
            // Mostrar teclado mientras no se supere el numero de intentos maximo (3)
            if (contador_intentos < 3)
            {
                pantalla_teclado(3);
                estado=18;
            }
            // Si se ha bloqueado la cerradura
            else
                estado=20;
            break;
            // Comprobacion de clave
        case 18:
            dib = 1;
            // Variable para comprobar la coincidencia del codigo
            clave_correcta=1;
            // Bucle de comprobacion de caracteres del codigo
            for(i=0;i<long_clave;i++)
            {
                // Si no coincide alguno de los caracteres, se pone a 0 "clave_correcta" y se sale del bucle
                if (buffer_clave[i] != rand_number[i])
                {
                    clave_correcta=0;
                    break;
                }
            }
            // Si la clave es correcta
            if (clave_correcta == 1)
            {
                ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Clave correcta");
                // Si es el admin
                if (numero_usuario == 0)
                {
                    // Al menu de admin
                    estado = 3;
                    BorrarBufferClave();
                }
                else
                    estado = 19;
                // Espera de 1 segundo
                esperar = 1;
            }
            // Si no es correcta
            else
            {
                ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Clave incorrecta");
                estado = 17;
                // Espera de 1 segundo
                esperar = 1;
                // Se borra buffer de clave para la nueva introduccion
                BorrarBufferClave();
                // Se aumenta contador de intentos
                contador_intentos++;
            }
            break;
            // Animacion de desbloqueo de puerta y apertura de puerta
        case 19:
            dib = 1;
            // Cargar imagen LOCK y dibujar
            ComColor(255,255,255);
            almacenar_imagen_externa(flash_addr_lock, ram_addr_BMP1, nbytes_lock);
            Nueva_pantalla(255,255,255);
            dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_lock, alto_pix_lock, HSIZE/2-ancho_pix_lock/2, VSIZE/2-alto_pix_lock/2, 255, 0);
            Dibuja();
            // Espera de 1 segundo
            EsperaSeg(1);
            // Cargar imagene DESBLOQUEO y dibujar
            almacenar_imagen_externa(flash_addr_unlock, ram_addr_BMP1, nbytes_unlock);
            Nueva_pantalla(255,255,255);
            dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_unlock, alto_pix_unlock, HSIZE/2-ancho_pix_unlock/2, VSIZE/2-alto_pix_unlock/2, 255, 0);
            Dibuja();
            // Servo abre la puerta
            PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, true);
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, Min_pos);
            // Espera de 3 segundos
            EsperaSeg(3);
            // Vuelta al inicio
            estado=0;
            // Se borran todos los buffers
            BorrarBufferCorreo();
            BorrarBufferNombre();
            BorrarBufferClave();
            // Servo cierra la puerta
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, Max_pos);
            EsperaSeg(1);
            PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT, false);
            break;
            // Mensaje de puerta bloqueada por exceso de intentos
        case 20:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"Numero de intentos superado"); //pinta un texto
            ComTXT(HSIZE/2,VSIZE*2/3, 21, OPT_CENTER,"Contacte con admin para desbloquear"); //pinta un texto
            // Se bloquea cerradura
            bloqueo = 1;
            // Espera de 2 segundos
            esperar=2;
            // Vuelta al inicio
            estado=21;
            break;
            // Vuelta a inicio y borrado de buffers
        case 21:
            dib = 0;
            // Vuelta al inicio
            estado=0;
            // Se borran todos los buffers
            BorrarBufferCorreo();
            BorrarBufferNombre();
            BorrarBufferClave();
            break;
        case 22:
            dib = 1;
            ComTXT(HSIZE/2,VSIZE/2, 28, OPT_CENTER,"No se ha podido enviar el correo, contacte con el administrador");
            // Espera de 1 segundo
            esperar = 3;
            estado = 21;
            break;
        }
        // Algunos estados no tienen "Dibujas()" dentro, por lo que se dibujaran al llegar aqui
        if (dib == 1)
            Dibuja();
        // Llamada a timer de X segundos si es necesario
        if (esperar > 0)
        {
            EsperaSeg(esperar);
            esperar = 0;
        }
    }
}

/* ----------------------------------------------------
 * ----------------------------------------------------
 * -------------BLOQUE DE CONFIGURACIONES--------------
 * ----------------------------------------------------
 * ---------------------------------------------------- */


// Configuracion timer
void config_timer1(int Reloj){
    uint32_t periodo1;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);           //Habilita T0
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);   //T0 a 120MHz
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);        //T0 periodico y conjunto (32b)
    periodo1 = 6000000;                                   //Periodo de 50 ms
    TimerLoadSet(TIMER0_BASE, TIMER_A, periodo1 -1);
    TimerIntRegister(TIMER0_BASE,TIMER_A,interrupcion_timer1);
    IntEnable(INT_TIMER0A);                                 //Habilitar las interrupciones globales de los timers
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);        // Habilitar las interrupciones de timeout
    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_TIMER0);       //Habilitar el timer 0 durante el Sleep
    SysCtlPeripheralClockGating(true);

}

// Configuración del PWM, utilizado para SERVO
void config_PWM(){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    PWMClockSet(PWM0_BASE,PWM_SYSCLK_DIV_64);   // al PWM le llega un reloj de 1.875MHz
    GPIOPinConfigure(GPIO_PG1_M0PWM5);          //Configurar el pin a PWM
    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    //Configurar el pwm0, contador descendente y sin sincronización (actualización automática)
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PeriodoPWM=37499; // 50Hz  a 1.875MHz
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, PeriodoPWM); //frec:50Hz
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, Max_pos);   //Inicialmente, 1ms
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);     //Habilita el generador 2
    //    PWMOutputState(PWM0_BASE, PWM_OUT_5_BIT , true);    //Habilita la salida 5
}

// Configuración de periféricos para el lector de tarjetas NFC RC522
void config_RC522(){
    uint32_t junkAuxVar;

    // Periférico 3 del SSI (entero en el boosterpack 2)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI3);

    // Reset
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // SDA, SCK, MOSI, MISO
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);


    GPIOPinConfigure(GPIO_PQ0_SSI3CLK);     // SCK
    GPIOPinConfigure(GPIO_PQ2_SSI3XDAT0);   // MOSI
    GPIOPinConfigure(GPIO_PQ3_SSI3XDAT1);   // MISO

    // SCK, MOSI, MISO
    GPIOPinTypeSSI(GPIO_PORTQ_BASE, GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3);

    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_4); //NRSTPD
    GPIOPinTypeGPIOOutput(GPIO_PORTQ_BASE, GPIO_PIN_1); //chipSelectPin

    // Configurar el reloj
    SSIConfigSetExpClk(SSI3_BASE, 160000000, SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 4000000, 8);

    // Activar el módulo SSI 3
    SSIEnable(SSI3_BASE);

    // Eliminar posibles "residuos"
    while(SSIDataGetNonBlocking(SSI3_BASE, &junkAuxVar)){}


    // Activar pin de CS y NRSTPD
    GPIOPinWrite(GPIO_PORTQ_BASE, chipSelectPin, chipSelectPin);
    GPIOPinWrite(GPIO_PORTB_BASE, NRSTPD, NRSTPD);

    // Inicializar el MFRC522 con los pines correspondientes
    Mfrc522(chipSelectPin,NRSTPD);


}



void pantalla_inicial(){
    Inicia_pantalla();
    // Note: Keep SPI below 11MHz here

    // =======================================================================
    // Delay before we begin to display anything
    // =======================================================================

    SysCtlDelay(RELOJ/3);

    // ================================================================================================================
    // PANTALLA INICIAL
    // ================================================================================================================

    // Fade-in de la pantalla
    VolNota(100);
    TocaNota(0x45,48);
    for(i=0;i<255;i+=5){
        Nueva_pantalla(i,i,i);
        Dibuja();
    }

    // Mostrar el logo inicial de TIVA LOCK
    almacenar_imagen_externa(flash_addr_logo, ram_addr_BMP1, nbytes_logo);
    for(i=0;i<255;i+=5)
    {
        Nueva_pantalla(255,255,255);
        dibujar_imagen_externa(ram_addr_BMP1, ancho_pix_logo, alto_pix_logo, HSIZE/2-ancho_pix_logo/2, VSIZE/2-alto_pix_logo/2, i, 0);
        Dibuja();
    }
    Espera_pant();

    // Calibrar la pantalla según se haya elegido la opción de 3.5" o 5"
#ifdef VM800B35
    for(i=0;i<6;i++)    Esc_Reg(REG_TOUCH_TRANSFORM_A+4*i, REG_CAL[i]);
#endif
#ifdef VM800B50
    for(i=0;i<6;i++)    Esc_Reg(REG_TOUCH_TRANSFORM_A+4*i, REG_CAL5[i]);
#endif
}

// Función encargada de hacer poll al lector RFID, almacena tarjeta leída
void RFID()
{
    // Ver si se detecta tarjeta
    status = Request(PICC_REQIDL, str);

    // En caso de conflictos
    status = Anticoll(str);

    // Copiar la cadena que tiene la ID de la tarjeta
    memcpy(cardID, str, 10);

    // Tarjeta detectada
    if(status == MI_OK){
        tarjeta_detectada=1;
    }
}

// Función para encontrar posiciones de memoria disponibles a la hora de almacenar nuevos usuarios
int EncontrarEspacio(int posicion)
{
    int espacio_libre = posicion;

    // Mientras la primera posición de cada bloque sea distinto de 0xFF, continuar buscando
    while( HWREGB(espacio_libre) != 0xFF)
        espacio_libre+=SaltoUsuarios;

    // Devuelve la posición encontrada
    return espacio_libre;
}

// Almacena la información relativa con el usuario según los parámetros recibidos
void GuardarUSUARIO(int posicion, unsigned char* ID_usuario, char* correo_usuario, char* nombre_usuario)
{
    int posicion_ini = posicion;
    int posicion_act = posicion;

    int long_nombre_usuario=0;
    int long_correo_usuario=0;

    char buff_correo[MAX_long_correo];
    char buff_nombre[MAX_long_nombre];

    // Copiar en el buffer interno las cadenas de correo y nombre de usuario
    for(j=0;j<MAX_long_correo;j++)
    {
        buff_correo[j] = *(correo_usuario + j);
    }
    for(j=0;j<MAX_long_nombre;j++)
    {
        buff_nombre[j] = *(nombre_usuario + j);
    }

    // Indicadores (PONER COMO DEFINES????)
    uint32_t indID=0x1;
    uint32_t indCorreo=0x2;
    uint32_t indNombre=0x3;

    // Para guardar id
    uint32_t numero;


    numero=0; // Poner a 0 ya que hacemos OR

    // Guardar indicador de ID y 3 primero digitos
    numero |= (uint8_t)indID;
    numero |= (uint8_t)ID_usuario[0]<<8*1;
    numero |= (uint8_t)ID_usuario[1]<<8*2;
    numero |= (uint8_t)ID_usuario[2]<<8*3;

    // Programar en la flash del microcontrolador
    FlashProgram(&numero, posicion_act, 4);

    // Repetir el proceso con los dos últimos byte, rellenar con 0xFF hasta formar una palabra (32 bits)
    numero = 0;
    // 4o y 5o digito
    numero |= (uint8_t)ID_usuario[3]<<8*0;
    numero |= 0xFFu<<8*1;
    numero |= 0xFFu<<8*2;
    numero |= 0xFFu<<8*3;
    FlashProgram(&numero, posicion_act+4, 4); // en el siguiente hueco

    // Calcular posición para almacenar correo
    posicion_act = posicion_ini + SaltoID;

    // Guardar correo
    byte_envio = 0;
    j=0;

    // Almacenar en buffer el correo
    while(buff_correo[long_correo_usuario] != 0)
        long_correo_usuario++;

    /*
     * Guardar en la flash. Procedimiento:
     *  - Mandar el indicador de correo
     *  - Guardar el correo
     *  Se programa cada 4 bytes, y al
     *  salir del bucle se comprueba si
     *  el último no se envió, por no
     *  llegar a 4B, y se rellena con 0xFF
     *  y se programa en la flash.
     */
    for(i=0;i<long_correo_usuario+2;i++)
    {
        // Preparar byte_envio para ser enviado
        if(i == 0)
            byte_envio |= indCorreo<<8*j;          // Indicador
        else if (i<long_correo_usuario+1)
            byte_envio |= buff_correo[i-1]<<8*j;   // Correo
        else
            byte_envio |= 128<<8*j;                // Byte de finalización de correo

        // Comprobación para mandar 4 bytes
        if (j<3)
            // Si es menor que 4 bytes, aumentar
            j++;
        else {
            // Si se llega a 4, programar, actualizar la posición y reiniciar las variables
            FlashProgram(&byte_envio, posicion_act, 4);
            j=0;
            posicion_act+=4;
            byte_envio = 0;
        }
    }
    // Comprobación del último byte
    for (i=0;i<4-j;i++){
        byte_envio |= 0xFF<<8*(j+i);
    }
    FlashProgram(&byte_envio, posicion_act, 4); //guardamos en la flash

    // Guardars indicador de nombre
    posicion_act = posicion_ini + SaltoCorreo + SaltoID;

    // Guardar nombre en buffer
    byte_envio = 0;
    while(buff_nombre[long_nombre_usuario] != 0)
        long_nombre_usuario++;

    // Reiniciar indice de 4 bytes
    j=0;

    // Realizar mismo procedimiento que con el buffer del correo electrónico
    for(i=0;i<long_nombre_usuario+2;i++)
    {
        if(i == 0)
            byte_envio |= indNombre<<8*j;
        else if (i<long_correo_usuario+1)
            byte_envio |= buff_nombre[i-1]<<8*j;
        else
            byte_envio |= 128<<8*j;

        if (j<3)
            j++;
        else {
            FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash
            j=0;
            posicion_act+=4;
            byte_envio = 0;
        }
    }
    for (i=0;i<4-j;i++){
        byte_envio |= 0xFF<<8*(j+i);
    }
    j=0;
    FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash
}

// Procedimiento idéntico al de guardar usuario, con la única diferencia de que la información viene pre-almacenada
void GuardarADMIN(int posicion)
{
    int posicion_ini = posicion;
    int posicion_act = posicion;

    unsigned char correo_admin[] = "smart.lock.tm4c@gmail.com";
    int long_correo_admin;

    unsigned char nombre_admin[] = "Administrador";
    int long_nombre_admin;

    uint32_t indID=0x1;
    uint32_t indCorreo=0x2;
    uint32_t indNombre=0x3;
//    unsigned char IDadmin[]={ 0x5a, 0x32, 0x9e, 0xd5 };
    unsigned char IDadmin[]={ 0x2b, 0xc, 0xcf, 0x22 };
    uint32_t numero; // para guardar id

    // Guardamos ID e indicador de ID
    numero=0; //ponemos a 0 ya que hacemos OR
    // id y 3 primero digitos
    numero|=(uint8_t)indID;
    numero|=(uint8_t)IDadmin[0]<<8*1; //pasamos de char a uint32
    numero|=(uint8_t)IDadmin[1]<<8*2;
    numero|=(uint8_t)IDadmin[2]<<8*3;

    FlashProgram(&numero,posicion_act,4);
    numero = 0;
    // 4o y 5o digito
    numero|=(uint8_t)IDadmin[3]<<8*0;
    numero|=0xFF<<8*1;
    numero|=0xFF<<8*2;
    numero|=0xFFu<<8*3;
    FlashProgram(&numero,posicion_act+4,4); // en el siguiente hueco

    // Guardamos indicador de correo
    posicion_act = posicion_ini + SaltoID;
    // Guardamos correo
    byte_envio = 0;
    j=0;
    long_correo_admin = sizeof(correo_admin)-1;
    for(i=0;i<long_correo_admin+1;i++)
    {
        if(i == 0)
            byte_envio |= indCorreo<<8*j;
        else if (i<long_correo_admin+1)
            byte_envio |= correo_admin[i-1]<<8*j;
        else
            byte_envio |= 128<<8*j;
        if (j<3)
            j++;
        else {
            FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash
            j=0;
            posicion_act+=4;
            byte_envio = 0;
        }
    }
    for (i=0;i<4-j;i++){
        byte_envio |= 0xFF<<8*(j+i);
    }
    j=0;
    FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash

    // Guardamos indicador de nombre
    posicion_act = posicion_ini + SaltoCorreo + SaltoID;

    // Guardamos nombre
    byte_envio = 0;
    long_nombre_admin = sizeof(nombre_admin)-1;
    for(i=0;i<long_nombre_admin+1;i++)
    {
        if(i == 0)
            byte_envio |= indNombre<<8*j;
        else if (i<long_nombre_admin+1)
            byte_envio |= nombre_admin[i-1]<<8*j;
        else
            byte_envio |= 128<<8*j;
        if (j<3)
            j++;
        else {
            FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash
            j=0;
            posicion_act+=4;
            byte_envio = 0;
        }
    }
    for (i=0;i<4-j;i++){
        byte_envio |= 0xFF<<8*(j+i);
    }
    j=0;
    FlashProgram(&byte_envio,posicion_act,4); //guardamos en la flash
}

// Función que obtiene la dirección de correo almacenada en flash
void ObtenerCorreo()
{
    posicion= InicioMemoria + numero_usuario * SaltoUsuarios + SaltoID + 1;
    i=0;
    while(HWREGB(posicion) != 0xFF)
    {
        byte_leido_flash=HWREGB(posicion); //obtiene el valor de la direccion especificada
        posicion++; //pasa al siguiente registro de memoria
        if (byte_leido_flash!=128)
            correorecuperado[i]=byte_leido_flash;
        else
            break;
        i++;
    }
}

// Mismo procedimiento que con void ObtenerCorreo();
void ObtenerNombre()
{
    posicion= InicioMemoria + numero_usuario * SaltoUsuarios + SaltoID + SaltoCorreo + 1;
    i=0;
    while(HWREGB(posicion) != 0xFF)
    {
        byte_leido_flash=HWREGB(posicion); //obtiene el valor de la direccion especificada
        posicion++; //pasa al siguiente registro de memoria
        if (byte_leido_flash!=128)
            nombrerecuperado[i]=byte_leido_flash;
        else
            break;
        i++;
    }
}

// BLOQUE DE ELIMINACIÓN DE BUFFERS DE CORREO, NOMBRE, NÚMERO ALEATORIO...
void BorrarBufferCorreo()
{
    for(i=0;i<MAX_long_correo;i++)
    {
        correorecuperado[i]=0;
    }
}

void BorrarBufferNombre()
{
    for(i=0;i<MAX_long_nombre;i++)
    {
        nombrerecuperado[i]=0;
    }
}

void BorrarBufferClave()
{
    for(i=0;i<long_clave;i++)
    {
        buffer_clave[i]=0;
    }
}
void BorrarBufferEscritura()
{
    for(i=0;i<MAX_long_correo;i++)
    {
        buffer_correo[i]=0;
        if(i<MAX_long_nombre)
            buffer_nombre[i]=0;
    }
}
void BorrarBufferEscrituraCorreo()
{
    for(i=0;i<MAX_long_correo;i++)
    {
        buffer_correo[i]=0;
    }
}
void BorrarBufferEscrituraNombre()
{
    for(i=0;i<MAX_long_nombre;i++)
    {
        buffer_nombre[i]=0;
    }
}



// Función para comprobar la ID, devuelve ok=x en función del resultado
int ComprobarID(int posicion)
{

    int posicion_id = posicion+1;
    int j;
    int ok=1;
    int IDflash;

    numero_usuario = 0;
    while(ok == 1)
    {
        // comparamos cada digito de la ID con la flash
        for (j=0;j<4;j++)
        {
            IDflash = HWREGB(posicion_id);
            // si coincide avanzamos a la siguiente posicion
            if (IDflash == cardID[j])
            {
                posicion_id++;
                // si han coincidido todos los numeros, lectura correcta , ok=2
                if(j==3)
                    ok = 2;
            }
            else
            {
                // si no ha coincidido el digito, pasamos a la siguiente ID
                posicion_id=posicion_id+SaltoUsuarios-j;
                // comprobamos si hay algo escrito, si no, no hay mas ID,
                // tarjeta no reconocida
                if(HWREGB(posicion_id) == 0xFF){
                    ok = 0;
                }
                else
                    numero_usuario++;
                //si hay otra ID posible volvemos al bucle y comprobamos con esta nueva ID
                break;
            }
        }
    }
    return ok;
}



// TECLADO
void pantalla_teclado(short int campo){
    //    Lee_pantalla();
    indice_teclado = 0;
    SHIFT = false;
    while(ENTER == false)
    {
        Nueva_pantalla(150, 150, 150);

        get_teclado(campo);

        pantalla(campo);

        Dibuja();
    }
    ENTER = false;
}

void get_teclado(short int campo){

    // Leemos el registro, almacenar en tecla
    tecla = Lee_Reg(REG_TOUCH_TAG);


    // Si se puede seguir escribiendo en el vector
    switch (campo){
    case 0:{
        // Si se ha pulsado la pantalla
        if(POSX!=0x8000){
            // Si la tecla entra dentro de la tabla ASCII básica
            if (tecla > 0 && tecla<128){
                if (indice_teclado<MAX_long_correo){
                    // Evita que mantener pulsada la tecla escriba más de una vez el valor en el vector
                    if (tecla!=tecla_p && tecla != 0){

                        TocaNota(0x50,60);
                        buffer_correo[indice_teclado]=tecla;
                        indice_teclado++;
                    }
                }
                else
                    indice_teclado=0;
            }
        }
        // Si se pulsa el botón de borrar
        if (DELETE == true){
            DELETE = false;
            // Si el indice_teclado es mayor que 0, disminuir
            if (indice_teclado>0)
            {

                TocaNota(0x50,60);
                indice_teclado--;
            }
            // Si es 0, saturar, evita que indice_teclado vaya a números negativos
            else
                indice_teclado =0;
            // Borrar (poner a 0) valor del buffer de correo
            buffer_correo[indice_teclado] = 0;
        }
        if(SPACE == true){
            SPACE = false;
            buffer_correo[indice_teclado]=' ';
            indice_teclado++;
        }
        break;
    }
    case 1:{
        // Si se ha pulsado la pantalla
        if(POSX!=0x8000){
            // Si la tecla entra dentro de la tabla ASCII básica
            if (tecla > 0 && tecla<128){
                if (indice_teclado<MAX_long_nombre){
                    // Evita que mantener pulsada la tecla escriba más de una vez el valor en el vector
                    if (tecla!=tecla_p && tecla != 0){

                        TocaNota(0x50,60);
                        buffer_nombre[indice_teclado]=tecla;
                        indice_teclado++;
                    }
                }
                else
                    indice_teclado=0;
            }
        }
        // Si se pulsa el botón de borrar
        if (DELETE == true){
            DELETE = false;
            // Si el indice_teclado es mayor que 0, disminuir
            if (indice_teclado>0)
            {

                TocaNota(0x50,60);
                indice_teclado--;
            }
            // Si es 0, saturar, evita que indice_teclado vaya a números negativos
            else
                indice_teclado =0;
            // Borrar (poner a 0) valor del buffer de correo
            buffer_nombre[indice_teclado] = 0;
        }
        if(SPACE == true){
            SPACE = false;
            buffer_nombre[indice_teclado]=' ';
            indice_teclado++;
        }
        break;
    }
    case 3:{
        // Si se ha pulsado la pantalla
        if(POSX!=0x8000){
            // Si la tecla entra dentro de la tabla ASCII básica
            if (tecla > 0 && tecla<128){
                if (indice_teclado<long_clave){
                    // Evita que mantener pulsada la tecla escriba más de una vez el valor en el vector
                    if (tecla!=tecla_p && tecla != 0){
                        TocaNota(0x50,60);
                        buffer_clave[indice_teclado]=tecla;
                        indice_teclado++;
                    }
                }
            }
        }
        // Si se pulsa el botón de borrar
        if (DELETE == true){
            DELETE = false;
            // Si el indice_teclado es mayor que 0, disminuir
            if (indice_teclado>0)
            {

                TocaNota(0x50,60);
                indice_teclado--;
            }
            // Si es 0, saturar, evita que indice_teclado vaya a números negativos
            else
                indice_teclado =0;
            // Borrar (poner a 0) valor del buffer de correo
            buffer_clave[indice_teclado] = 0;
        }
        if(SPACE == true){
            SPACE = false;
            buffer_clave[indice_teclado]=' ';
            indice_teclado++;
        }
        break;
    }
    }

    // Actualizar valor de tecla_p
    tecla_p = tecla;
    if (ENTER == true){
        //        ENTER = false;
        indice_teclado = 0;
        // Meter llamada a escribir en flash el correo
    }
}


short int rect_h, rect_w, rect_x, rect_y;

void pantalla(short int campo){
    /*
     * BLOQUE DE CALCULOS
     */

    // Separacion entre teclado y borde pantalla
    tecla_borde = 2*fila1_inicio_x;  // pix

    /*
     * Calculo de tamaño de tecla "estándar". Teniendo en cuenta que no se manda OPT_x.
     * Se calcula con la fila superior, y se ajustan las demás.
     * (Ancho pantalla - espacio entre teclado y borde - 3 * n letras)/(n letras)
     */
    tecla_w = (HSIZE - tecla_borde - 3 * (n_tecla_fila_1 - 1)) / n_tecla_fila_1;  // Ancho teclas segun fila 1

    // Calculo inicio filas 0, 2 y 3
    fila0_inicio_y = VSIZE - n_filas * (tecla_h + tecla_separacion);

    fila1_inicio_y = fila0_inicio_y + tecla_separacion + tecla_h;

    fila2_inicio_x = HSIZE / 2 - tecla_w * n_tecla_fila_2 / 2 - tecla_borde / 2;  // Inicio fila 2
    fila2_inicio_y = fila1_inicio_y + tecla_separacion + tecla_h;

    fila3_inicio_x = HSIZE / 2 - tecla_w * n_tecla_fila_3 / 2 - tecla_borde / 2;  // Inicio fila 3
    fila3_inicio_y = fila1_inicio_y + 2*tecla_separacion + 2*tecla_h;

    fila4_inicio_y = fila1_inicio_y + 3*tecla_separacion + 3*tecla_h;

    // Calculo anchura rectángulos
    fila1_rect_w = n_tecla_fila_1 * (3 + tecla_w) - 3;  // El 3 sale del tamaño entre botones (VER DATASHEET, PAG 187 FT800 PROGRAMMER GUIDE)
    fila2_rect_w = n_tecla_fila_2 * (3 + tecla_w) - 3;  // El 3 sale del tamaño entre botones (VER DATASHEET, PAG 187 FT800 PROGRAMMER GUIDE)
    fila3_rect_w = n_tecla_fila_3 * (3 + tecla_w) - 3;  // El 3 sale del tamaño entre botones (VER DATASHEET, PAG 187 FT800 PROGRAMMER GUIDE)


    /*
     * Pintar la cadena hasta el momento.
     * Añadir un rectángulo sobre el texto.
     */
    rect_w = fila1_rect_w;
    rect_h = tecla_h;
    rect_x = fila1_inicio_x;
    rect_y = fila0_inicio_y - rect_h - 5;


    // Rectángulo
    Comando(CMD_BEGIN_RECTS);

    ComVertex2ff(rect_x, rect_y);
    ComVertex2ff(rect_x + rect_w, rect_y + rect_h);

    ComColor(255, 210, 201);
    //    ComVertex2ff(rect_x + rect_w, rect_y + rect_h);
    //    ComVertex2ff(rect_x + rect_w, rect_y);
    Comando(CMD_END);

    /*
     * Comprobar si se ha escrito algo. En caso
     * afirmativo, mostrar por pantalla. En caso
     * negativo, mostrar un mensaje invitando a hacerlo.
     */


    switch (campo){
    case 0:{
        if (indice_teclado != 0){
            ComColor(0,0,0);
            ComTXT(HSIZE/2, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTER, buffer_correo);
        }
        else{
            ComColor(201,201,201);
            ComTXT(fila1_inicio_x + 5, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTERY, "Introduzca su correo");
        }
        break;
    }
    case 1:{
        if (indice_teclado != 0){
            ComColor(0,0,0);
            ComTXT(HSIZE/2, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTER, buffer_nombre);
        }
        else{
            ComColor(201,201,201);
            ComTXT(fila1_inicio_x + 5, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTERY, "Introduzca nombre y apellidos");
        }
        break;
    }
    case 3:{
        if (indice_teclado != 0){
            ComColor(0,0,0);
            ComTXT(HSIZE/2, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTER, buffer_clave);
        }
        else{
            ComColor(201,201,201);
            ComTXT(fila1_inicio_x + 5, fila0_inicio_y - 5 - rect_h / 2, 21, OPT_CENTERY, "Introduzca clave recibida");
        }
        break;
    }
    }

    ComColor(0xff,0xff,0xff);

    /*
     * Para incluir las teclas se utiliza la función Boton, a la que le entran
     * las posiciones x,y; la altura y anchura, y la cadena a mostrar. La función
     * devuelve si se está pulsando el botón, y comparando con el estado anterior,
     * se decide si situar las variables correspondiente a true o false.
     */

    // Incluir tecla shift
    shift_boton = Boton(fila1_inicio_x, fila3_inicio_y, (HSIZE - fila3_rect_w - tecla_borde) / 2 - 3, tecla_h, 21, "Blq May");
    if (shift_boton == 1 && shift_boton_p == 0 && SHIFT == false){

        TocaNota(0x50,50);
        SHIFT = true;
    }
    else if (shift_boton == 1 && shift_boton_p == 0 && SHIFT == true){

        TocaNota(0x50,50);
        SHIFT = false;
    }
    shift_boton_p = shift_boton;

    // Incluir tecla options
    options_boton = Boton(fila1_inicio_x, fila4_inicio_y, (HSIZE - fila3_rect_w - tecla_borde) / 2 - 3, tecla_h, 21, "?123");
    if (options_boton == 1 && options_boton_p == 0 && OPTIONS == false){

        TocaNota(0x50,50);
        OPTIONS = true;
    }
    else if (options_boton == 1 && options_boton_p == 0 && OPTIONS == true){

        TocaNota(0x50,50);
        OPTIONS = false;
    }
    options_boton_p = options_boton;

    // Incluir tecla de enter
    enter_boton = Boton(fila3_inicio_x + fila3_rect_w + 3, fila4_inicio_y, (HSIZE - fila3_rect_w - tecla_borde) / 2 - 3, tecla_h, 21, "Enter");
    if (enter_boton == 1 && enter_boton_p == 0){

        TocaNota(0x50,50);
        ENTER = true;
    }
    enter_boton_p = enter_boton;

    // Incluir tecla de borrar
    delete_boton = Boton(fila3_inicio_x + fila3_rect_w + 3, fila3_inicio_y, (HSIZE - fila3_rect_w - tecla_borde) / 2 - 3, tecla_h, 21, "DEL");
    if (delete_boton == 1 && delete_boton_p == 0){
        DELETE = true;
    }
    delete_boton_p = delete_boton;



    // Si se ha pulsado una tecla válida, almacenarla en la variable para que se muestre sin efecto 3D
    int letra_pulsada = 0;
    if (tecla > 0 && tecla<128)
        letra_pulsada = (int)tecla;
    else
        letra_pulsada = 0;

    /*
     * Mostrar por pantalla cada una de las filas del teclado,
     * teniendo en cuenta el estado de SHIFT, OPTIONS Y ENTER.
     */
    if (OPTIONS == true){
        ComTeclas(fila1_inicio_x, fila0_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila0);
        ComTeclas(fila1_inicio_x, fila1_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila1_especiales);
        ComTeclas(fila2_inicio_x, fila2_inicio_y, fila2_rect_w, tecla_h, 21, letra_pulsada, fila2_especiales);
        ComTeclas(fila3_inicio_x, fila3_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila3_especiales);
        ComTeclas(fila3_inicio_x, fila4_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila4);
    }
    else if (SHIFT == true || campo == 3){
        ComTeclas(fila1_inicio_x, fila0_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila0);
        ComTeclas(fila1_inicio_x, fila1_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila1_mayus);
        ComTeclas(fila2_inicio_x, fila2_inicio_y, fila2_rect_w, tecla_h, 21, letra_pulsada, fila2_mayus);
        ComTeclas(fila3_inicio_x, fila3_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila3_mayus);
        ComTeclas(fila3_inicio_x, fila4_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila4);
    }
    else{
        ComTeclas(fila1_inicio_x, fila0_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila0);
        ComTeclas(fila1_inicio_x, fila1_inicio_y, fila1_rect_w, tecla_h, 21, letra_pulsada, fila1_minus);
        ComTeclas(fila2_inicio_x, fila2_inicio_y, fila2_rect_w, tecla_h, 21, letra_pulsada, fila2_minus);
        ComTeclas(fila3_inicio_x, fila3_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila3_minus);
        ComTeclas(fila3_inicio_x, fila4_inicio_y, fila3_rect_w, tecla_h, 21, letra_pulsada, fila4);
    }
    // Incluir tecla de espacio
    space_boton = Boton(fila3_inicio_x + fila3_rect_w / n_tecla_fila_3, fila4_inicio_y-1, fila3_rect_w * 5/7, tecla_h+2,21," ");
    if (space_boton == 1 && space_boton_p == 0){
        TocaNota(0x50,60);
        SPACE = true;
    }
    space_boton_p = space_boton;
}

// Funcion para contar segundos
void EsperaSeg( int segundos )
{
    // Reiniciar variable
    tiempo = 0;
    // segundos a esperar = 20 * segundos pues la interrupion es cada 50ms
    while(tiempo < segundos*20);
}



