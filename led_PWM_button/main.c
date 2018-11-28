#include "MDR32Fx.h"
#include "core_cm3.h"
#include "MDR32F9Qx_config.h"
#include "system_MDR32F9Qx.h"
#include "MDR32F9Qx_rst_clk.h"
#include "MDR32F9Qx_port.h"
#include <MDR32F9Qx_timer.h>
#include "stdlib.h"
#include "stdio.h"
#include <stdbool.h>
#include "MDR32F9Qx_adc.h"


#define delay(T) for(i = T; i > 0; i--)
int i;
// коэффициент для преобразования кода с АЦП в вольты (подбирается)
#define SCALE 1240

// Необходимые для инициализации структуры
// Самого АЦП
ADC_InitTypeDef ADC;
// Нужного нам канала
ADCx_InitTypeDef ADC1;
// для таймера
TIMER_CntInitTypeDef TIM1Init;

void ADCInit() {
	// включение тактирования АЦП
	RST_CLK_PCLKcmd(RST_CLK_PCLK_RST_CLK | RST_CLK_PCLK_ADC, ENABLE);
	// заполнение структуры с настройкми АЦП значениями по умолчанию
	ADC_StructInit(&ADC);
	// и инициализация АЦП этими значениями
	ADC_Init(&ADC);
	// заполнение структуры с настройками канала АЦП значениями по умолчанию
	ADCx_StructInit(&ADC1);
	// ВЫбираем канал 7 (порт PD7).
	// Перемычку входа АЦП XP2 нужно переставить в положение TRIM.
	ADC1.ADC_ChannelNumber = ADC_CH_ADC7;
	// настройка 7-го канала АЦП.
	ADC1_Init(&ADC1);
	// Включение и выставление наивысшего приоритета прерыванию от АЦП
	// в настройках NVIC контроллера
	NVIC_EnableIRQ(ADC_IRQn);
	NVIC_SetPriority(ADC_IRQn, 0);
	// включение прерывания от АЦП по завершению преобразования.
	ADC1_ITConfig(ADCx_IT_END_OF_CONVERSION, ENABLE);
	// включение работы АЦП1
	ADC1_Cmd(ENABLE);
}

bool conInProgress;
unsigned int rawResult;
unsigned char channel;
float result;

void ADC_IRQHandler() {
	// если прерывание по завершению преобразования, обрабатываем результат
	if (ADC_GetITStatus(ADC1_IT_END_OF_CONVERSION)) {
		// читаем регистр с результатами преобразования
		rawResult = ADC1_GetResult();
		// Получение номера канала, завершившего преобразование
		// биты 16..20 регистра
		channel = (rawResult & 0x1F0000) >> 16;
		// оставляем первые 12 бит регистра результата,
		// в которых содержится сам результат преобразвоания
		rawResult &= 0x00FFF;
		// преобразуем результат в вольты
		result = (float) rawResult / (float) SCALE;

		TIM1Init.TIMER_Period = 200*result;
		TIMER_CntInit(MDR_TIMER1, &TIM1Init);
		//TIMER_ITConfig(MDR_TIMER1, TIMER_STATUS_CNT_ZERO, ENABLE);
		//TIMER_Cmd(MDR_TIMER1, ENABLE);

		printf("Напряжение на переменном резистора %fВ (канал АЦП %i)\n",
				result, channel);
		fflush(stdout);
		conInProgress = false;
		// снимаем флаг ожидания прерывания АЦП
		NVIC_ClearPendingIRQ(ADC_IRQn);
	}
}

void leds_init(void);
void timer_init(void);
void change_led(void);

int led_state;

int main() {
	leds_init();
	// Настраиваем и запускаем АЦП
	ADCInit();
	timer_init();
	led_state = 0;
	result = 1.0;



	//бесконечный цикл
	while (1) {
		// После задержки если преобразование завершилось, запускаем заново
		delay(0xFFFF);

		if (!conInProgress) {
			ADC1_Start();
			conInProgress = true;
		}
	}
}

void leds_init(void) {
	RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTA , ENABLE);

	PORT_InitTypeDef PortInit; //объявление структуры PortInit
	//Инициализация порта A на выход
	// направление передачи данных = Выход
	PortInit.PORT_OE = PORT_OE_OUT;
	// режим работы вывода порта = Порт
	PortInit.PORT_FUNC = PORT_FUNC_PORT;
	// режим работы вывода = Цифровой
	PortInit.PORT_MODE = PORT_MODE_DIGITAL;
	// скорость фронта вывода = медленный
	PortInit.PORT_SPEED = PORT_SPEED_SLOW;
	// выбор всех выводов для инициализации
	PortInit.PORT_Pin = (PORT_Pin_1 | PORT_Pin_2);
	//инициализация заданными параметрами порта C
	PORT_Init(MDR_PORTA, &PortInit);
}

void timer_init(void) {

	// Включение тактирования таймера
	RST_CLK_PCLKcmd(RST_CLK_PCLK_TIMER1, ENABLE);
	// Настройка делителя тактовой частоты для таймера
	TIMER_BRGInit(MDR_TIMER1, TIMER_HCLKdiv1);
	//Получение в структуру настроек по умолчанию
	TIMER_CntStructInit(&TIM1Init);
	//Задание предделителя тактовой частоты (раз в 8000 тактов
	// счётчик таймера увеличвается на 1)
	TIM1Init.TIMER_Prescaler = 8000;
	//Задание модуля счета (когда счётчик дойдёт до 1000, сработает прерывание)
	TIM1Init.TIMER_Period = 200;
	// Настройка таймера с выбранными настройками предделителя тактовой
	// частоты и модуля счёта
	TIMER_CntInit(MDR_TIMER1, &TIM1Init);
	// Включение прерываний от таймера 1
	NVIC_EnableIRQ(TIMER1_IRQn);
	//Установка приоритета прерываний
	NVIC_SetPriority(TIMER1_IRQn, 0);
	TIMER_ITConfig(MDR_TIMER1, TIMER_STATUS_CNT_ZERO, ENABLE);
	TIMER_Cmd(MDR_TIMER1, ENABLE);
}

void Timer1_IRQHandler(void) {
	if (TIMER_GetITStatus(MDR_TIMER1, TIMER_STATUS_CNT_ZERO)) {
		change_led();
		// Очистка флага прерывания в таймере для предотвращения
		// повторного вызова того же прерывания)
		TIMER_ClearITPendingBit(MDR_TIMER1, TIMER_STATUS_CNT_ZERO);
	}
}

void change_led(void) {
	if (led_state == 0) {
		PORT_SetBits(MDR_PORTA, PORT_Pin_1);
		PORT_ResetBits(MDR_PORTA, PORT_Pin_2);
		led_state = 1;
	} else {
		PORT_SetBits(MDR_PORTA, PORT_Pin_2);
		PORT_ResetBits(MDR_PORTA, PORT_Pin_1);
		led_state = 0;
	}
}

void exit(int code) {
}
