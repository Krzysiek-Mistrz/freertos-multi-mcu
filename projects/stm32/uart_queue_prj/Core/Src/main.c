/* USER CODE BEGIN Header */
/*
producer 0.2s ---|
producer 0.5s ---+--> queue of msg_t --> receiver task --> USART2 --> PC
producer 3s   ---|
*/
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// @brief phase enum is 4 producer 2 indicate if it is 
// starting a long task, ending long task, or neither. 
// receiver can use this information 2 know when producer 
// is busy and when it is done
typedef enum {
    PHASE_NONE = 0, // 1st is 0, next are 1, 2..
    PHASE_START,
    PHASE_END
} phase_t;

// @brief msg_t is message type that is sent 4m producer 2 receiver
typedef struct {
    uint32_t period_ms;
    uint32_t timestamp_ms;
    uint8_t phase;
} msg_t;

// @brief producer_cfg_t is configuration struct that is passed 2 producer tas
typedef struct {
    uint32_t period_ms;
} producer_cfg_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
producer 0.2s ---|
producer 0.5s ---+--> queue of msg_t --> receiver task --> USART2 --> PC
producer 3s   ---|
*/

#ifndef RX_MODE_BATCH
#define RX_MODE_BATCH 0
#endif
#ifndef LONG_TASK // long_task is 4 producer 2 simulate a long task that takes LONG_TASK_MS ms to complete
#define LONG_TASK 0
#endif
#ifndef LONG_TASK_MS
#define LONG_TASK_MS 300U
#endif

#define QUEUE_LEN 64U // quqe len is in number of msg_t, not bytes
#define RX_BATCH_PERIOD 5000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// global variables type shi...
static QueueHandle_t q_queue; // our queue 2 which producers send messages & receiver receives messages
static volatile uint32_t g_dropped; // count of msgs being dripped because of full queue
static const producer_cfg_t k_cfg_200 = { .period_ms = 200U };
static const producer_cfg_t k_cfg_500 = { .period_ms = 500U };
static const producer_cfg_t k_cfg_3000 = { .period_ms = 3000U };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// returns ms since TIM2 was started (replaces board_ms())
static inline uint32_t app_ms(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

// blocking uart write via HAL (replaces board_uart_write())
static void app_uart_write(const char *s) {
    HAL_UART_Transmit(&huart2, (const uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

// @brief post is function that is called upn by producer task 2 send msg 2 queue
static void post(uint32_t period_ms, phase_t phase) {
    msg_t m = { period_ms, app_ms(), (uint8_t)phase };

    if (xQueueSend(q_queue, &m, 0) != pdPASS) {
        g_dropped++; // 4 show dropped messages type shi..
    }
}

// @brief producer_task is task that runs in a loop, waits for period_ms, and sends msg 2 queue
static void producer_task(void *arg) {
    const uint32_t period = ((const producer_cfg_t *)arg)->period_ms;
    TickType_t last = xTaskGetTickCount(); // gettin current tick count 4m scheduler start

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(period)); // delay curr task until in abs time last + pdMS_TO_TICKS(period
#if LONG_TASK
        if (period == 500U) {
            post(period, PHASE_START); // send start msg 2 receiver
            uint32_t t0 = app_ms();
            while ((app_ms() - t0) < LONG_TASK_MS) { }
            post(period, PHASE_END); // send end msg 2 receiver
            continue; 
        }
#endif
        post(period, PHASE_NONE); // send msg 2 receiver
    }
}

// @brief fmt_period formats the period in ms into string repr
static int fmt_period(char *out, size_t n, uint32_t ms) {
    uint32_t s = ms / 1000U, f = ms % 1000U; // where f is the fractional part of the seconds

    if (f == 0U)
        return snprintf(out, n, "%lus", (unsigned long)s); // if no fractional part, just print seconds, cause snprintf is used just 2 format the string..

    char frac[4];
    snprintf(frac, sizeof(frac), "%03lu", (unsigned long)f); // format fractional part with leading zeros

    // remove trailing zeros from fractional part
    for (int i = 2; i > 0 && frac[i] == '0'; i--)
        frac[i] = '\0';

    return snprintf(out, n, "%lu.%ss", (unsigned long)s, frac); // print seconds and fractional part
}

// @brief fmt_time formats timestamp in ms into string repr
static int fmt_time(char *out, size_t n, uint32_t ms) {
    uint32_t s = ms / 1000U, f = ms % 1000U;

    if (s == 0U)
        return snprintf(out, n, "%lums", (unsigned long)f);
    if (f == 0U)
        return snprintf(out, n, "%lus", (unsigned long)s);

    return snprintf(out, n, "%lus %lums", (unsigned long)s, (unsigned long)f);
}

// @brief print_msg formats and prints the message to UART
static void print_msg(const msg_t *m) {
    // we have 1d arra of const char pointers so we cannot do 
    // tag[x][y] = z as well as tag[x] = "y"
    static const char *const tag[] = {
        "",
        " start",
        " end"
    };
    char per[16], ts[24], line[80]; // buffers 4 formatted period, timestamp, and final line
    fmt_period(per, sizeof(per), m->period_ms);
    fmt_time(ts, sizeof(ts), m->timestamp_ms);
    // format the final line with task period, phase, and timestamp
    snprintf(line, sizeof(line), "Task %s%s - %s\r\n", per, tag[m->phase <= PHASE_END ? m->phase : 0], ts);
    app_uart_write(line); // write the line 2 uart
}

static void receiver_task(void *arg) {
    (void)arg; // unused parameter
    msg_t m;
    uint32_t reported_drops = 0U;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
#if RX_MODE_BATCH
        // we just wait for the batch period and then read all messages in the queue & send em
        vTaskDelayUntil(&last, pdMS_TO_TICKS(RX_BATCH_PERIOD)); // wait for batch period
        while (xQueueReceive(q_queue, &m, 0) == pdPASS) {
            print_msg(&m); // print each message in the batch -> and also send 2 uart
        }
#else
        // we just wait for a message and print it immediately
        (void)last;

        if (xQueueReceive(q_queue, &m, portMAX_DELAY) == pdPASS) {
            print_msg(&m);
        }
#endif
        uint32_t d = g_dropped;

        if (d != reported_drops) {
            char line[40];
            snprintf(line, sizeof(line), "dropped msgs count -> %lu\r\n", (unsigned long)d);
            app_uart_write(line);
            reported_drops = d;
        }
    }
}

// called from MX_FREERTOS_Init() in freertos.c (replaces the body of your old main())
void app_start(void) {
    q_queue = xQueueCreate(QUEUE_LEN, sizeof(msg_t)); // create the queue with QUEUE_LEN and size 4 each element being sizeof(msg_t)
    configASSERT(q_queue != NULL);
    vQueueAddToRegistry(q_queue, "msgq");

    xTaskCreate(producer_task, "p200", 256, (void *)&k_cfg_200, 4, NULL); // k_cfg_200 is 
    xTaskCreate(producer_task, "p500", 256, (void *)&k_cfg_500, 3, NULL);
    xTaskCreate(producer_task, "p3000", 256, (void *)&k_cfg_3000, 2, NULL);
    xTaskCreate(receiver_task, "rx", 384, NULL, 1, NULL);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim2);   /* ms counter starts here */

  app_uart_write("\r\n--- uart_queue_prj: "
#if RX_MODE_BATCH
                 "RX every 5 s"
#else
                 "RX immediate"
#endif
#if LONG_TASK
                 ", long 0.5s task"
#endif
                 " ---\r\n");
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration (HSI16 -> PLL -> 80 MHz)
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
