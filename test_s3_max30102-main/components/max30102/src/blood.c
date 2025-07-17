/*
 * @Author: mojionghao
 * @Date: 2024-08-12 16:47:41
 * @LastEditors: mojionghao
 * @LastEditTime: 2025-06-13 21:03:14
 * @FilePath: \test_s3_max30102\components\max30102\src\blood.c
 * @Description:
 */
#include "blood.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "max30102.h"
// uint16_t g_fft_index = 0;
struct compx s1[FFT_N + 16];
struct compx s2[FFT_N + 16];
float Data_heart, Data_spo2;
float bpm[10];
float BPM;
    int i = 0;
    float a;
float BPM_MAX = 0;
float BPM_MIN = 999;
#define MIN_PEAK_INTERVAL_MS 300  // 300 ms
#define MAX_PEAK_INTERVAL_MS 2000 // 2000 ms
#define MIN_PEAK_COUNT 3          // 最少两个峰
#define SAMPLE_TARGET FFT_N
uint32_t ir_data[SAMPLE_TARGET];
uint32_t timestamp_data[SAMPLE_TARGET];
int hr_index = 0; // 心率采样计数器

int heart_rate_calc(uint32_t *ir_data, uint32_t *timestamp_data, int len);

uint16_t g_fft_index = 0;
struct
{
    float Hp;
    float HPO2;
} g_BloodWave;

BloodData g_blooddata = {0}; // 血液数据存储

#define CORRECTED_VALUE 47 // 标定血液氧气含量

/*funcation start ------------------------------------------------------------*/
// 血液检测信息更新
void blood_data_update(max30102_handle_t sensor)
{

    uint16_t fifo_red = 0, fifo_ir = 0;
    max30102_dev_t *sens = (max30102_dev_t *)sensor;
    while (g_fft_index < FFT_N)
    {
        while (gpio_get_level(sens->int_pin) == 0)
        {
            max30102_read_fifo(sensor, &fifo_red, &fifo_ir);
            // printf("FIFO IR: %d \n", fifo_ir);
            if (g_fft_index < FFT_N)
            {
                s1[g_fft_index].real = fifo_red;
                s1[g_fft_index].imag = 0;

                s2[g_fft_index].real = fifo_ir;
                s2[g_fft_index].imag = 0;

                g_fft_index++;
            }
            // ========== 心率数据 ==========
            if (hr_index < SAMPLE_TARGET)
            {
                ir_data[hr_index] = fifo_ir;
                timestamp_data[hr_index] = esp_timer_get_time() / 1000; // ms
                hr_index++;
            }

            // ========== 判断是否采集完成 ==========
            if (hr_index >= SAMPLE_TARGET)
            {
                if(i<=10&&((a= heart_rate_calc(ir_data, timestamp_data, SAMPLE_TARGET))>0)){
                    bpm[i]= a;
                    BPM_MAX = a>BPM_MAX ? a:BPM_MAX;
                    BPM_MIN = a<BPM_MIN ? a:BPM_MIN;
                    i++;
                    printf("i:%d\n",i);
                }
                hr_index = 0; // 可选择重新采样或只运行一次
            }
            if(i==10){
                float sum= 0;
                for (int j = 0;j<10; j++){
                    sum +=bpm[j];
                }
                g_blooddata.heart = (sum - BPM_MAX - BPM_MIN)/8;
                printf("平均BPM: %.2f,最大：%.2f,最小:%.2f\n",BPM,BPM_MAX,BPM_MIN);
                i=0;
            }
        }
    }
}

void blood_data_translate(void)
{
    float n_denom;
    uint16_t i;

    // 直流滤波
    float dc_red = 0;
    float dc_ir = 0;
    float ac_red = 0;
    float ac_ir = 0;

    for (i = 0; i < FFT_N; i++)
    {
        dc_red += s1[i].real;
        dc_ir += s2[i].real;
    }
    dc_red = dc_red / FFT_N;
    dc_ir = dc_ir / FFT_N;
    for (i = 0; i < FFT_N; i++)
    {
        s1[i].real = s1[i].real - dc_red;
        s2[i].real = s2[i].real - dc_ir;
    }

    // 移动平均滤波

    for (i = 1; i < FFT_N - 1; i++)
    {
        n_denom = (s1[i - 1].real + 2 * s1[i].real + s1[i + 1].real);
        s1[i].real = n_denom / 4.00;

        n_denom = (s2[i - 1].real + 2 * s2[i].real + s2[i + 1].real);
        s2[i].real = n_denom / 4.00;
    }
    for (i = 0; i < FFT_N - 8; i++)
    {
        n_denom = (s1[i].real + s1[i + 1].real + s1[i + 2].real + s1[i + 3].real + s1[i + 4].real + s1[i + 5].real + s1[i + 6].real + s1[i + 7].real);
        s1[i].real = n_denom / 8.00;

        n_denom = (s2[i].real + s2[i + 1].real + s2[i + 2].real + s2[i + 3].real + s2[i + 4].real + s2[i + 5].real + s2[i + 6].real + s2[i + 7].real);
        s2[i].real = n_denom / 8.00;
    }

    g_fft_index = 0;
    FFT(s1);
    FFT(s2);
    for (i = 0; i < FFT_N; i++)
    {
        s1[i].real = sqrtf(s1[i].real * s1[i].real + s1[i].imag * s1[i].imag);
        s1[i].real = sqrtf(s2[i].real * s2[i].real + s2[i].imag * s2[i].imag);
    }
    for (i = 1; i < FFT_N; i++)
    {
        ac_red += s1[i].real;
        ac_ir += s2[i].real;
    }
    for (i = 0; i < 50; i++)
    {
        if (s1[i].real <= 10)
            break;
    }
    // 读取峰值点的横坐标  结果的物理意义为
    int s1_max_index = find_max_num_index(s1, 30);
    int s2_max_index = find_max_num_index(s2, 30);
    //	UsartPrintf(USART_DEBUG,"%d\r\n",s1_max_index);
    //	UsartPrintf(USART_DEBUG,"%d\r\n",s2_max_index);
    float Heart_Rate = 60.00 * ((100.0 * s1_max_index) / 512.00) + 20;
    float Heart_Rate_s2 = 60.0 * ((100.0 * s2_max_index) / 512.0) + 20;
    //g_blooddata.heart = s2[g_fft_index].real;
    float R = (ac_ir * dc_red) / (ac_red * dc_ir);
    float sp02_num = -45.060 * R * R + 30.354 * R + 94.845;
    g_blooddata.SpO2 = sp02_num;
}

void blood_Loop(max30102_handle_t sensor, float *heart, float *spo2)
{
    blood_data_update(sensor);
    blood_data_translate();
    g_blooddata.SpO2 = (g_blooddata.SpO2 > 99.99) ? 99.99 : g_blooddata.SpO2;
    if (isnan(g_blooddata.SpO2) || g_blooddata.heart == 66)
    {
        g_blooddata.SpO2 = 0;
        ESP_LOGE("blood", "No human body detected!");
    }
    *heart = g_blooddata.heart;
    *spo2 = g_blooddata.SpO2;
}

int heart_rate_calc(uint32_t *ir_data, uint32_t *timestamp_data, int len)
{
    if (len < 3)
    {
        printf("错误： 计算心率的数据点不足。\n");
        return -1;
    }

    // —— 1. 简单滑动平均滤波 ——
    float *filtered = malloc(len * sizeof(float));
    const int win = 5; // 5 点滑动窗口
    for (int i = 0; i < len; i++)
    {
        int count = 0;
        float sum = 0;
        for (int j = i - win / 2; j <= i + win / 2; j++)
        {
            if (j >= 0 && j < len)
            {
                sum += ir_data[j];
                count++;
            }
        }
        filtered[i] = sum / count;
    }

    // —— 2. 动态阈值：平均值 + 标准差 ——
    float mean = 0, sd = 0;
    for (int i = 0; i < len; i++)
        mean += filtered[i];
    mean /= len;
    for (int i = 0; i < len; i++)
        sd += (filtered[i] - mean) * (filtered[i] - mean);
    sd = sqrtf(sd / len);
    float threshold = mean + sd;

    // —— 3. 峰值检测 & 峰间隔累加 ——
    int last_peak_time = -1;
    int peak_count = 0;
    int interval_sum = 0;

    for (int i = 1; i < len - 1; i++)
    {
        // 局部极大 + 超过阈值
        if (filtered[i] > filtered[i - 1] &&
            filtered[i] > filtered[i + 1] &&
            filtered[i] > threshold)
        {
            int t = timestamp_data[i];
            if (last_peak_time != -1)
            {
                int interval = t - last_peak_time;
                // 只统计合理的峰间隔
                if (interval >= MIN_PEAK_INTERVAL_MS &&
                    interval <= MAX_PEAK_INTERVAL_MS)
                {
                    interval_sum += interval;
                    peak_count++;
                }
            }
            last_peak_time = t;
        }
    }

    free(filtered);

    // 不足峰数则返回错误
    if (peak_count < MIN_PEAK_COUNT)
    {
        printf("错误： 检测到的峰值数量不足。\n");
        return -1;
    }

    // 平均间隔（ms）
    float avg_interval = interval_sum / (float)peak_count;
    // 转换为 BPM
    int bpm_no = (int)(60000.0f / avg_interval + 0.5f);
    printf("Peak count: %d, Avg interval: %.2f ms, BPM: %d\n",
           peak_count, avg_interval, bpm_no);
    return bpm_no;
}
