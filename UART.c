#include <avr/io.h>
#define F_CPU 16000000UL
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7

// Prototypes
// Initialization
void UART_Ini(uint8_t com, uint32_t baudrate, uint8_t size, uint8_t parity, uint8_t stop);

// Send
void UART_puts(uint8_t com, char *str);
void UART_putchar(uint8_t com, char data);

// Received
uint8_t UART_available(uint8_t com);
char UART_getchar(uint8_t com );
void UART_gets(uint8_t com, char *str);

void UART_clrscr( uint8_t com );
void UART_setColor(uint8_t com, uint8_t color);
void UART_gotoxy(uint8_t com, uint8_t x, uint8_t y);

// Utils
void itoa(uint16_t number, char* str, uint8_t base);
uint16_t atoi(char *str);

typedef struct {
	uint8_t  ucsra;
	uint8_t  ucsrb;
	uint8_t  ucsrc;
	uint8_t  reserved;
	uint8_t  ubrrl;
	uint8_t  ubrrh;
	uint8_t  udr;
} reg_UART;

static volatile reg_UART * const uartsList[] = {
	(volatile reg_UART*)&UCSR0A,
	(volatile reg_UART*)&UCSR1A,
	(volatile reg_UART*)&UCSR2A,
	(volatile reg_UART*)&UCSR3A,
};

void UART_Ini(uint8_t com, uint32_t baudrate, uint8_t size, uint8_t parity, uint8_t stop)
{
	if (com > 3) return;

	volatile reg_UART *uart = uartsList[com];

	uint32_t ubrr32 = (F_CPU + (16UL*baudrate)/2) / (16UL*baudrate) - 1UL;
	uint16_t ubrr = (uint16_t)ubrr32;

	uart->ucsra &= ~(1 << U2X0);

	uart->ubrrl = (uint8_t)(ubrr & 0xFF);
	uart->ubrrh = (uint8_t)((ubrr >> 8) & 0x0F);

	uart->ucsrb &= ~(1 << UCSZ02);
	uart->ucsrc &= ~((1 << UCSZ01) | (1 << UCSZ00) | (1 << UPM01) | (1 << UPM00) | (1 << USBS0));

	if (size < 5 || size > 9) size = 8;
	switch (size) {
		case 5: break;
		case 6: uart->ucsrc |= (1 << UCSZ00); break;
		case 7: uart->ucsrc |= (1 << UCSZ01); break;
		case 8: uart->ucsrc |= (1 << UCSZ01) | (1 << UCSZ00); break;
		case 9: uart->ucsrc |= (1 << UCSZ01) | (1 << UCSZ00); uart->ucsrb |= (1 << UCSZ02); break;
	}

	if (parity == 1) {
		uart->ucsrc |= (1 << UPM01) | (1 << UPM00);
		} else if (parity == 2) {
		uart->ucsrc |= (1 << UPM01);
	}

	if (stop == 2) uart->ucsrc |= (1 << USBS0);
	else uart->ucsrc &= ~(1 << USBS0);

	uart->ucsrb |= (1 << RXEN0) | (1 << TXEN0);
}

uint8_t UART_available(uint8_t com){
	if(com > 3) return 0;
	
	volatile reg_UART *uart = uartsList[com];
	
	return (uart->ucsra & (1 << RXC0));
}

char UART_getchar(uint8_t com){
	if(com > 3) return 0;
	
	while(!(UART_available(com)))
		;
		
	volatile reg_UART *uart = uartsList[com];
	return (char)uart->udr;
}

void UART_putchar(uint8_t com, char data){
	if(com > 3) return;
	
	volatile reg_UART *uart = uartsList[com];
	while(!(uart->ucsra & (1 << UDRE0))){
		;
	}
	uart->udr = data;
}

void UART_puts(uint8_t com, char *str){
	while (*str) {
		UART_putchar(com, *str++);
	}
}

void UART_gets(uint8_t com, char *str)
{
    if (com > 3) { if (str) str[0] = '\0'; return; }

    uint8_t i = 0;

    UART_puts(com, "\x1B[s");
    UART_puts(com, "\x1B[K");

    for (;;) {
        char c = UART_getchar(com);

        if (c == '\r' || c == '\n') {
            str[i] = '\0';
            break;

        } else if (c == 8 || c == 127) {
            if (i > 0) {
                i--;
                str[i] = '\0';
                UART_puts(com, "\b \b");
            } else {
                UART_puts(com, "\x1B[u");
            }

        } else if (i < 19) {
            str[i++] = c;
            UART_putchar(com, c);
        }
    }
}


void UART_clrscr(uint8_t com){ 
	UART_puts(com, "\x1B[2J\x1B[H"); 
}

static const uint8_t ansiColors[] = {30,31,32,33,34,35,36,37};

void UART_setColor(uint8_t com, uint8_t color)
{
	if (com > 3) return;
	if (color >= (sizeof(ansiColors) / sizeof(ansiColors[0]))) color = 7;

	char buf[8];
	itoa(ansiColors[color], buf, 10);
	UART_puts(com, "\x1B[");
	UART_puts(com, buf);
	UART_putchar(com, 'm');
}


void UART_gotoxy(uint8_t com, uint8_t x, uint8_t y)
{
	if (com > 3) return;
	char buf[8];

	UART_puts(com, "\x1B[");
	itoa(y, buf, 10); UART_puts(com, buf);
	UART_putchar(com, ';');
	itoa(x, buf, 10); UART_puts(com, buf);
	UART_putchar(com, 'H');
}


void itoa(uint16_t v, char* str, uint8_t base)
{
	char tmp[17];
	uint8_t i = 0;
	if (base < 2) base = 10;

	do {
		uint16_t r = v % base;
		tmp[i++] = (r < 10) ? ('0' + r) : ('A' + r - 10);
		v /= base;
	} while (v && i < sizeof(tmp));

	uint8_t j = 0;
	while (i) str[j++] = tmp[--i];
	str[j] = '\0';
}

uint16_t atoi(char *s)
{
	uint16_t v = 0;
	while (*s && *s != '.') {
		if (*s >= '0' && *s <= '9')
		v = (uint16_t)(v * 10 + (*s - '0'));
		else
		break;
		s++;
	}
	return v;
}
