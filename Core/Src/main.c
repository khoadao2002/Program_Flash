/*
 * main.c
 *
 *  Created on: Oct 28, 2023
 *      Author: khoad
 */
#include "main.h"
#include "string.h"

typedef enum
{
	Sector_0,
	Sector_1,
	Sector_2,
	Sector_3,
	Sector_4,
	Sector_5,
	Sector_6,
	Sector_7,
}Sector_t;
char tx_buffer[] = "Hello STM32!!!\r\n";
char rx_buffer[5888] = {0};
int count = 0;
unsigned int tick = 0;
void Init_Systick()
{
	unsigned int* SYST_CSR = (unsigned int*)0xE000E010;
	unsigned int* SYST_RVR = (unsigned int*)0xE000E014;
	*SYST_RVR = 16000/1 - 1;
	*SYST_CSR |= (1 << 0)|(1 << 1);
}
void SysTick_Handler_Custom()
{
	tick++;
	unsigned int* SYST_CSR = (unsigned int*)0xE000E010;
	*SYST_CSR &= ~(1 << 16);
}
void delay_ms(unsigned int time)
{
	tick = 0;
	while(tick < time);
}
void Init_UART()
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	//Configure PA2 & PA3 at alternate function mode
	unsigned int* GPIOA_MODER = (unsigned int*)(0x40020000 + 0x00);
	unsigned int* GPIOA_AFRL = (unsigned int*)(0x40020000 + 0x20);
	*GPIOA_MODER |= (0b10 << 4)|(0b10 << 6);
	*GPIOA_AFRL |= (7 << 8)|(7 << 12);

	__HAL_RCC_USART2_CLK_ENABLE();
	//Configure register for UART2
	unsigned int* UART_BRR = (unsigned int*)(0x40004400 + 0x08);
	unsigned int* UART_CR1 = (unsigned int*)(0x40004400 + 0x0C);
	unsigned int* UART_CR3 = (unsigned int*)(0x40004400 + 0x14);
	*UART_BRR = (104 << 4)|(3 << 0); //Baund Rate: 9600(Bps)
	*UART_CR1 |= (1 << 13)|(1 << 3)|(1 << 2);
	*UART_CR3 |= (1 << 6);
}

void Init_DMA()
{
	__HAL_RCC_DMA1_CLK_ENABLE();
	unsigned int* DMA_S5PAR = (unsigned int*)(0x40026000 + 0x18 + (0x18*5));
	unsigned int* DMA_S5M0AR = (unsigned int*)(0x40026000 + 0x1C + (0x18*5));
	unsigned int* DMA_S5NDTR = (unsigned int*)(0x40026000 + 0x14 + (0x18*5));
	unsigned int* DMA_S5CR = (unsigned int*)(0x40026000 + 0x10 + (0x18*5));
	unsigned int* NVIC_ISER0 =(unsigned int*)(0xE000E100);
	*DMA_S5PAR = (0x40004400 + 0x04);
	*DMA_S5M0AR = (unsigned int)rx_buffer;
	*DMA_S5NDTR = sizeof(rx_buffer);
	*DMA_S5CR |= (1 << 10)|(1 << 4)|(1 << 0)|(4 << 25);
	*NVIC_ISER0 |= (1 << 16);
}
//Interrupt for RXNE
void DMA_Handler_Custom()
{
	count = 1;
	unsigned int* DMA_HIFCR = (unsigned int*)(0x40026000 + 0x0C);
	*DMA_HIFCR |= (1 << 11);
}
//Transmission one byte data
void Transmission_one_Byte(char data)
{
	unsigned int* UART_SR = (unsigned int*)(0x40004400 + 0x00);
	unsigned int* UART_DR = (unsigned int*)(0x40004400 + 0x04);
	while(((*UART_SR >> 7) & 1) != 1);
	*UART_DR = data;
	while(((*UART_SR >> 6) & 1) != 1);
}

//Transmission multiple byte data
void Transmission_Data(char* data)
{
	for(int i = 0; data[i] != '\0'; i++)
	{
		Transmission_one_Byte(data[i]);
	}
}

// Erase Flash
__attribute__((section(".Function_ram"))) void Erase_Flash(Sector_t Sector)
{
	unsigned int* FLASH_SR = (unsigned int*)(0x40023C00 + 0x0C);
	unsigned int* FLASH_CR = (unsigned int*)(0x40023C00 + 0x10);
	unsigned int* FLASH_KEYR = (unsigned int*)(0x40023C00 + 0x04);
	while(((*FLASH_SR >> 16) & 1) == 1);
	if(((*FLASH_CR >> 31) & 1) == 1)
	{
		*FLASH_KEYR = 0x45670123;
		*FLASH_KEYR = 0xCDEF89AB;
	}
	*FLASH_CR |= (1 << 1)|(Sector << 3)|(1 << 16);
	while(((*FLASH_SR >> 16) & 1) == 1);
	*FLASH_CR &= ~(1 << 1);
}

//Program FLASH
__attribute__((section(".Function_ram"))) void Program_FLASH(void* addr, char* data, unsigned int size)
{
	unsigned int* FLASH_SR = (unsigned int*)(0x40023C00 + 0x0C);
	unsigned int* FLASH_CR = (unsigned int*)(0x40023C00 + 0x10);
	unsigned int* FLASH_KEYR = (unsigned int*)(0x40023C00 + 0x04);
	while(((*FLASH_SR >> 16) & 1) == 1);
	if(((*FLASH_CR >> 31) & 1) == 1)
	{
		*FLASH_KEYR = 0x45670123;
		*FLASH_KEYR = 0xCDEF89AB;
	}
	*FLASH_CR |= (1 << 0);
	char* write_pointer = addr;
	for(int i = 0; i < size; i++)
	{
		*write_pointer = data[i];
		while(((*FLASH_SR >> 16) & 1) == 1);
		write_pointer++;
	}
	*FLASH_CR &= ~(1 << 0);
}

__attribute__((section(".Function_ram"))) void update_firmware()
{
	unsigned int* SYST_CSR = (unsigned int*)0xE000E010;
	*SYST_CSR = 0;
	Erase_Flash(Sector_0);
	Program_FLASH((void*)0x08000000, rx_buffer, sizeof(rx_buffer));
	unsigned int* AIRCR = (unsigned int*)0xE000ED0C;
	*AIRCR = (0x5FA << 16)|(1 << 2);
}
int main()
{
	Init_Systick();
	Init_UART();
	Init_DMA();
	char* flash = 0;
	char* ram = (char*)0x20000000;
	for(int i = 0; i <= 0x198; i++)
	{
		ram[i] = flash[i];
	}
	unsigned int* VTOR = (unsigned int*)0xE000ED08;
	*VTOR = 0x20000000;
	//Create address for Systick_Handler
	unsigned int* temp = (unsigned int*)0x2000003C;
	*temp = (unsigned int)SysTick_Handler_Custom | 1;
	//Generate address for DMA_Handler
	unsigned int* dma = (unsigned int*)0x20000080;
	*dma = (unsigned int)DMA_Handler_Custom | 1;

	while(1)
	{
		if(count == 1)
			update_firmware();
	}
	return 0;
}

