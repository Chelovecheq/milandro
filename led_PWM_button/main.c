#include "MDR32Fx.h"
#include "core_cm3.h"
#include "MDR32F9Qx_config.h"
#include "system_MDR32F9Qx.h"
#include "MDR32F9Qx_rst_clk.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_timer.h"
#include "MDR32F9Qx_adc.h" //ADC
#include <stdbool.h>
#include "stdlib.h"
#include "stdio.h"

#define LED1 PORT_Pin_1 //определить нулевой вывод как LED1
#define LED2 PORT_Pin_2

static PORT_InitTypeDef PortInit; //объявление структуры PortInit
#define delay(T) for(i = T; i > 0; i--)

int i;
// коэффициент для преобразования кода с АЦП в вольты (подбирается)
#define SCALE 1240

// Необходимые для инициализации структуры
// Самого АЦП
ADC_InitTypeDef ADC;
// Нужного нам канала
ADCx_InitTypeDef ADC1;

void timer_init(void);
void pins_init(void);
void ADCInit(void);
void ADC_IRQHandler(void);

bool conInProgress;
unsigned int rawResult;
unsigned char channel;
float result;
uint16_t resulto;


int main()
{
  //включить тактирование портов С, B, E, A
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTC, ENABLE);
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTB, ENABLE);
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTE, ENABLE);
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTA, ENABLE);

  timer_init();
  pins_init();
  buttons_init();
  ADCInit();

  while (1)
  {
	delay(0xFFFF);
	     if (!conInProgress)
	     {
	       ADC1_Start();
	       conInProgress = true;
	     }
	//если нажат SELECT
    if (PORT_ReadInputDataBit(MDR_PORTC, PORT_Pin_2) == Bit_RESET)
    {
      // включить светодиоды
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL1, 0xFFF);
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL2, 0xFFF);
      printf("Select pressed\n");
      fflush(stdout);
    }
    //если нажат DOWN
    else if (PORT_ReadInputDataBit(MDR_PORTE, PORT_Pin_1) == Bit_RESET)
        {
          // включить светодиоды
          TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL1, 0x007);
          TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL2, 0x007);
          printf("Down pressed\n");
          fflush(stdout);
        }
    else if (PORT_ReadInputDataBit(MDR_PORTB, PORT_Pin_6) == Bit_RESET)
    {
      resulto = 0xFFF*result;
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL1, 0x007);
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL2, resulto);
      printf("Right pressed %i\n", resulto);
      fflush(stdout);
    }

    else if (PORT_ReadInputDataBit(MDR_PORTE, PORT_Pin_3) == Bit_RESET)
    {
      resulto = 0xFFF*result;
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL1, resulto);
      TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL2, 0x007);
      printf("Left pressed %i\n", resulto);
      fflush(stdout);
    }
  }
}

void pins_init(void)
{
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTA, ENABLE);

  PORT_InitTypeDef PortInit; //объявление структуры PortInit
  // направление передачи данных = Выход
  PortInit.PORT_OE = PORT_OE_OUT;
  // режим работы вывода порта = Порт
  PortInit.PORT_FUNC = PORT_FUNC_ALTER;
  // режим работы вывода = Цифровой
  PortInit.PORT_MODE = PORT_MODE_DIGITAL;
  // скорость фронта вывода = медленный
  PortInit.PORT_SPEED = PORT_SPEED_FAST;
  // выбор всех выводов для инициализации
  PortInit.PORT_Pin = (PORT_Pin_1 | PORT_Pin_3);
  //инициализация заданными параметрами порта C
  PORT_Init(MDR_PORTA, &PortInit);
}

void timer_init(void)
 {
   // Структуры для инициализации таймера
   TIMER_CntInitTypeDef sTIM_CntInit;
   TIMER_ChnInitTypeDef sTIM_ChnInit;
   TIMER_ChnOutInitTypeDef sTIM_ChnOutInit;

   // Длительности импульса для каналов 1 и 2
   uint16_t VAL1 = 0x007;
   uint16_t VAL2 = 0x3FF;

   // Включение тактирования таймера 1
   RST_CLK_PCLKcmd(RST_CLK_PCLK_TIMER1,ENABLE);
   /* Сброс установок таймера 1 */
   TIMER_DeInit(MDR_TIMER1);
   // Заполнение структуры значениями по умолчанию
   TIMER_CntStructInit(&sTIM_CntInit);
   // Предделитель частоты таймера равен 1 то есть частота таймера равна частоте микроконтоллера.
   sTIM_CntInit.TIMER_Prescaler = 0x0;
   // Модуль счета = 0xFFF
   // Период импульсов ШИМ = 0xFFF / частота счета таймера .
   sTIM_CntInit.TIMER_Period = 0xFFF;
   // Инициализация таймера 1
   TIMER_CntInit (MDR_TIMER1,&sTIM_CntInit);
   /* Инициализация каналов таймера 1: СН1,СН1N,СН2,СН2N */
   TIMER_ChnStructInit(&sTIM_ChnInit);
   // Режим работы канала - генерация ШИМ
   sTIM_ChnInit.TIMER_CH_Mode = TIMER_CH_MODE_PWM;
   // Формат выработки сигнала REF номер 6:
   // при счете вверх – REF = 1 если CNT<CCR, иначе REF = 0;
   // значение регистра CCR определяет длительность импульса ШИМ
   sTIM_ChnInit.TIMER_CH_REF_Format = TIMER_CH_REF_Format6;
   // Инициализируем канал 1
   sTIM_ChnInit.TIMER_CH_Number = TIMER_CHANNEL1;
   TIMER_ChnInit(MDR_TIMER1, &sTIM_ChnInit);
   // Инициализируем канал 2
   sTIM_ChnInit.TIMER_CH_Number = TIMER_CHANNEL2;

   TIMER_ChnInit(MDR_TIMER1, &sTIM_ChnInit);
   // Устанавливаем длительность импульсов по каждому каналу
   TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL1, VAL1);
   TIMER_SetChnCompare(MDR_TIMER1, TIMER_CHANNEL2, VAL2);
   /* Инициализация прямого и инверсного выходов (стр289 описания!!!)
   для каждого из каналов таймера 1: СH1,СH1N,СH2,СH2N */
   // Заполнение элементов структуры значениями по умолчанию
   TIMER_ChnOutStructInit(&sTIM_ChnOutInit);
   // Выбор источника сигнала для прямого выхода CHxN - сигнал REF
   sTIM_ChnOutInit.TIMER_CH_DirOut_Source = TIMER_CH_OutSrc_REF;
   // Настройна прямого выхода микроконтроллера CHxN на вывод данных
   sTIM_ChnOutInit.TIMER_CH_DirOut_Mode = TIMER_CH_OutMode_Output;
   // Выбор источника сигнала для инверсного выхода CHxN - сигнал REF
   sTIM_ChnOutInit.TIMER_CH_NegOut_Source = TIMER_CH_OutSrc_REF;
   // Настройна инверсного выхода микроконтроллера CHxN на вывод данных
   sTIM_ChnOutInit.TIMER_CH_NegOut_Mode = TIMER_CH_OutMode_Output;
   // Настраиваем выходы канала 1
   sTIM_ChnOutInit.TIMER_CH_Number = TIMER_CHANNEL1;
   TIMER_ChnOutInit(MDR_TIMER1, &sTIM_ChnOutInit);
   // Настраиваем выходы канала 2
   sTIM_ChnOutInit.TIMER_CH_Number = TIMER_CHANNEL2;
   TIMER_ChnOutInit(MDR_TIMER1, &sTIM_ChnOutInit);
   /* Включаем делитель тактовой частоты таймера 1*/
   TIMER_BRGInit(MDR_TIMER1,TIMER_HCLKdiv1);
   /* После всех настроек разрешаем работу таймера 1 */
   TIMER_Cmd(MDR_TIMER1,ENABLE);
 }

void buttons_init(void) {
	//Инициализация порта кнопки SELECT (С2) на вход
	  // направление передачи данных = вход
	  PortInit.PORT_OE = PORT_OE_IN;
	  // режим работы вывода порта = Порт
	  PortInit.PORT_FUNC = PORT_FUNC_PORT;
	  // режим работы выводе =цифровой
	  PortInit.PORT_MODE = PORT_MODE_DIGITAL;
	  // скорость фронта вывода= медленный
	  PortInit.PORT_SPEED = PORT_SPEED_SLOW;
	  // выбор вывода 2 для инициализации
	  PortInit.PORT_Pin = PORT_Pin_2;
	  //инициализация порта С заданными параметрами
	  PORT_Init(MDR_PORTC, &PortInit);

	  //Инициализация порта кнопки RIGHT (B6) на вход
	  // направление передачи данных = вход
	  PortInit.PORT_OE = PORT_OE_IN;
	  // режим работы вывода порта = Порт
	  PortInit.PORT_FUNC = PORT_FUNC_PORT;
	  // режим работы выводе =цифровой
	  PortInit.PORT_MODE = PORT_MODE_DIGITAL;
	  // скорость фронта вывода= медленный
	  PortInit.PORT_SPEED = PORT_SPEED_SLOW;
	  // выбор вывода 6 для инициализации
	  PortInit.PORT_Pin = PORT_Pin_6;
	  //инициализация порта С заданными параметрами
	  PORT_Init(MDR_PORTB, &PortInit);

	  //Инициализация порта кнопки LEFT (E3) на вход
	  // направление передачи данных = вход
	  PortInit.PORT_OE = PORT_OE_IN;
	  // режим работы вывода порта = Порт
	  PortInit.PORT_FUNC = PORT_FUNC_PORT;
	  // режим работы выводе =цифровой
	  PortInit.PORT_MODE = PORT_MODE_DIGITAL;
	  // скорость фронта вывода= медленный
	  PortInit.PORT_SPEED = PORT_SPEED_SLOW;
	  // выбор вывода 3 для инициализации
	  PortInit.PORT_Pin = PORT_Pin_3;
	  //инициализация порта С заданными параметрами
	  PORT_Init(MDR_PORTE, &PortInit);

	  //Инициализация порта кнопки DOWN (E1) на вход
	  	  // направление передачи данных = вход
	  	  PortInit.PORT_OE = PORT_OE_IN;
	  	  // режим работы вывода порта = Порт
	  	  PortInit.PORT_FUNC = PORT_FUNC_PORT;
	  	  // режим работы выводе =цифровой
	  	  PortInit.PORT_MODE = PORT_MODE_DIGITAL;
	  	  // скорость фронта вывода= медленный
	  	  PortInit.PORT_SPEED = PORT_SPEED_SLOW;
	  	  // выбор вывода 1 для инициализации
	  	  PortInit.PORT_Pin = PORT_Pin_1;
	  	  //инициализация порта С заданными параметрами
	  	  PORT_Init(MDR_PORTE, &PortInit);
}

void ADCInit()
{
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

void ADC_IRQHandler()
{
  // если прерывание по завершению преобразования, обрабатываем результат
  if (ADC_GetITStatus(ADC1_IT_END_OF_CONVERSION))
  {
    // читаем регистр с результатами преобразования
    rawResult = ADC1_GetResult();
    // Получение номера канала, завершившего преобразование
    // биты 16..20 регистра
    channel = (rawResult & 0x1F0000) >> 16;
    // оставляем первые 12 бит регистра результата,
    // в которых содержится сам результат преобразвоания
    rawResult &= 0x00FFF;
    // преобразуем результат в вольты
    result = (float) rawResult / (float) 4096;

    //printf("Напряжение на переменном резистора %fВ (канал АЦП %i)\n",
    //    result,channel);
    fflush(stdout);
    conInProgress = false;
    // снимаем флаг ожидания прерывания АЦП
    NVIC_ClearPendingIRQ(ADC_IRQn);
  }
}

void exit(int code)
{
}
