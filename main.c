/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include <stdio.h>

int16 result0 = 0,result1 = 0,result2 = 0,flag=0;
uint8 dataReady = 0;
int diff[2][2]={0},RL[5]={0};
float integralL,integralR;

#define SPEED 550 //モーターの速さ
#define MIDDLE 1000 //白と黒の中間値
#define KP 0.32//比例
#define KI 400//積分
#define KD 0.000008//微分
#define T 0.00001//周期

CY_ISR( ADC_SAR_Seq_1_ISR_LOC ) {
    uint32 intr_status;
    /* Read interrupt status register */
    intr_status = ADC_SAR_Seq_1_SAR_INTR_REG;

    /* Place your code here */
    result0 = ADC_SAR_Seq_1_GetResult16(0);//p[0]
    result1 = ADC_SAR_Seq_1_GetResult16(1);//p[1]
    result2 = ADC_SAR_Seq_1_GetResult16(2);//p[2]
    dataReady = 1;

    /* Clear hadled interrupt */
    ADC_SAR_Seq_1_SAR_INTR_REG = intr_status;
}

void forward(int left,int right){//正面
    PWM_3_WriteCompare(0);
    PWM_4_WriteCompare(0);
    PWM_1_WriteCompare(SPEED+left);
    PWM_2_WriteCompare(SPEED+right);
}

void start(){
    PWM_3_WriteCompare(0);
    PWM_4_WriteCompare(0);
    for(int i=0;i!=1000;i+=100){
        PWM_1_WriteCompare(i);
        PWM_2_WriteCompare(i);
        CyDelay(15);
    }
}
void left_t(){
    int16 newReading_left = 0;
    PWM_1_WriteCompare(0);
    PWM_4_WriteCompare(0);

    CyDelay(15);
    while(newReading_left<1200){
        PWM_2_WriteCompare(200);
        PWM_3_WriteCompare(400);
        if(dataReady != 0){
            dataReady = 0;
            newReading_left = result0;//p[1]
        }
    }
}
void right_t(){
    int16 newReading_right = 0;
    PWM_2_WriteCompare(0);
    PWM_3_WriteCompare(0);

    CyDelay(15);
    while(newReading_right<1200){
        PWM_1_WriteCompare(200);
        PWM_4_WriteCompare(400);
        if(dataReady != 0){
            dataReady = 0;
            newReading_right = result2;//p[1]
        }
    }
}
void stop(){
    PWM_1_WriteCompare(0);
    PWM_2_WriteCompare(0);
    PWM_3_WriteCompare(0);
    PWM_4_WriteCompare(0);
}
void back(){
    PWM_1_WriteCompare(0);
    PWM_2_WriteCompare(0);
    PWM_3_WriteCompare(1000);
    PWM_4_WriteCompare(1000);
    CyDelay(100);
}
void queue(int a){
    int i;

    for(i=0;i<4;i++){
        RL[i]=RL[i+1];
    }
    RL[4]=a;
}
int sum(){
    int i,sum=0;

    for(i=0;i<=4;i++){
        sum += RL[i];
    }

    return sum;
}

int main(void)
{
    int16 newReading_left = 0,newReading_center = 0,newReading_right = 0;
    char uart_line[256];
    int leftp=0,rightp=0,lefti=0,righti=0,leftd=0,rightd=0,leftF=0,rightF=0,i=0;

    CyGlobalIntEnable; /* Enable global interrupts. */
    PWM_1_Start();//左前モーター
    PWM_2_Start();//右前モーター
    PWM_3_Start();//左後モーター
    PWM_4_Start();//右後モーター
    /* Place your initialization/startup code here (e.g. MyInst_Start()) */



    UART_1_Start();
    UART_1_UartPutString("UART Starting...");
    UART_1_UartPutCRLF(0);

    /* Init and start SAR ADC データシートのpp.22-23を参照してください． */
    ADC_SAR_Seq_1_Start(); /* Initialize ADC */
    ADC_SAR_Seq_1_IRQ_Enable(); /* Enable ADC interrupts */
    ADC_SAR_Seq_1_StartConvert(); /* Start ADC conversions */

    /* Enable interrupt and set interrupt handler to local routine */
    ADC_SAR_Seq_1_IRQ_StartEx(ADC_SAR_Seq_1_ISR_LOC);

    start();
    for(;;)
    {

        if(dataReady != 0){
            dataReady = 0;
            newReading_left = result0;//p[0]
            newReading_center = result1;//p[1]
            newReading_right = result2;//p[2]
        }

    	  diff[0][0] = diff[0][1];//左
    	  diff[1][0] = diff[1][1];//右

    	  diff[0][1] = MIDDLE - newReading_left;//偏差
    	  diff[1][1] = MIDDLE - newReading_right;//

      	integralL += (diff[0][0] + diff[0][1]) / 2 * T;//積分
      	integralR += (diff[1][0] + diff[1][1]) / 2 * T;

        leftp = KP * diff[0][1];//P制御
        rightp = KP * diff[1][1];

		    lefti = KI * integralL;//I制御
		    righti = KI * integralR;

    	  leftd = KD * (diff[0][1] - diff[0][0]) / T;//D制御
    	  rightd = KD * (diff[1][1] - diff[1][0]) / T;

        leftF=leftp + lefti + leftd;
        rightF=rightp + righti + rightd;


        if(leftF>400){
            leftF=400;
        }
        if(rightF>400){
            rightF=400;
        }
        if(leftF<-600){
            leftF=-600;
        }
        if(rightF<-600){
            rightF=-600;
        }

        if(newReading_left>1100||newReading_center>1100||newReading_right>1100){
            forward(leftF,rightF);
            if(newReading_left<newReading_right){//次に曲がる方向を判定
                queue(1);
            }else{
                queue(-1);
            }
        }else{
            flag=sum();
            if(flag>0){
                right_t();
            }else{
                left_t();
            }
        }

        if(i>10){
            integralL = 0;
    	      integralR = 0;
            i=0;
        }


        //sprintf(uart_line, "ADC:%4d %4d %4d", newReading_left,newReading_center,newReading_right);
        UART_1_UartPutString(uart_line);
        UART_1_UartPutCRLF(0);
        i++;
    }
}
