const duty_max       = 0xE0;
const duty_min       = 0x70;
const wait_delay     = 0xA0;
const char output[] = {
                       0b10000000,
                       0b10100000,
                       0b00100000,
                       0b01100000,
                       0b01000000,
                       0b01010000,
                       0b00010000,
                       0b10010000
                      };


char delay_lo_cur   = 0x00;
char delay_hi_cur   = 0x00;
char current_state  = 0x00;
char tmp            = 0x00;

void  Interrupt() iv 0x0004 ics ICS_OFF {
//обработчик переполнения таймер0 для формирования гарантированой
//подзарядки бутстеп емкостей ~4 кГц
//открытие всех нижних плеч и потом возврат с текущему состоянию
//счетчик сбрасывается прерываниями по току которые сами выполняют эту функцию
if(INTCON.T0IF){
   tmp = PORTB;
   PORTB = 0b00000000;
   asm{NOP};
   asm{NOP};
   asm{NOP};
   PORTB = 0b10100000;
   asm{NOP};
   asm{NOP};
   asm{NOP};
   PORTB = 0b00000000;
   asm{NOP};
   asm{NOP};
   asm{NOP};
   PORTB = 0b01010000;
   asm{NOP};
   asm{NOP};
   asm{NOP};
   PORTB = tmp;
   INTCON.T0IF=0;
}

//прерывание от компараторов контроль тока
//сравнение со входом ref A2 формируется цепочкой
//PWM->фильтр->подстроечник
//изменение заполнения PWM управляет максимальным током
//если текущее состояние подразумевает ток в плече то высокий уровень компаратора
//сигналит о превышении тока и необходимости отключить плечо
//низкий уровень в тех же условиях требует включения плеча
//так же любым состоянием сбрасывается счетчик таймера0
if(PIR1.CMIF){
   // формируем выходные сигналы в соответствии с текущим состоянием
   tmp = output[current_state];
   if(CMCON.C1OUT) //вход компаратора для канала AB  вход RA0/AN0
   { //ток в плече AB выше установленного отключить
     tmp &= (~((1 <<  4)|(1 << 5)));
   }
   else
   { //ток в плече AB ниже заданного включить
     //действие не требуется сигнал включения сформирован выше
   }
   if(CMCON.C2OUT) //вход компаратора для канала CD  вход RA1/AN1
   { //ток в плече CD выше установленного отключить
     tmp &= (~((1 <<  6)|(1 << 7)));
   }
   else
   { //ток в плече CD ниже заданного включить
     //действие не требуется сигнал включения сформирован выше
   }
   TMR0=0x00;
   PORTB = tmp;
   PIR1.CMIF=0;
}

//внешнее прерывание "шаг"
//изменяет текущее состояние опираясь на вход "dir"
//расширяет заполнение PWM формирующего опорное напряжения для компаратора
//сбрасывает счетчик "бездействие" в ноль
if(INTCON.INTF){
   if(PORTB.B0==0){
     CCPR1L=duty_max;
     delay_hi_cur = 0;
     delay_lo_cur = 0;
     if(PORTB.B1==1)
     {//направление "вперед"
       current_state++;
       current_state &= 0x07;
     }
     else
     {//направление "назад"
       current_state--;
       current_state &= 0x07;
     }
   }
   tmp = output[current_state];
   PORTB = 0b00000000;
   asm{NOP};
   asm{NOP};
   asm{NOP};
   PORTB = tmp;
   INTCON.INTF=0;
}

//в каждом цикле PWM генерации увеличивается счетчик "бездействие".
//при достижении порога запрлнение PWM уменьшается тем самым
//формируется ток удержания, который ниже рабочего
//возврат к рабочему значению по спаду сигнала шаг.
if(PIR1.TMR2IF){
   if(delay_hi_cur > wait_delay){
     CCPR1L=duty_min;
   }
   else
   {
     delay_lo_cur++;
     if (delay_lo_cur==0){
       delay_hi_cur++;
     }
     
   }
   PIR1.TMR2IF=0;
}
}

void main() {
   //порт B первым шагом в "0"
   PORTB             = 0x00;
   TRISB             = 0x03;

   INTCON            = 0x00;
   OPTION_REG        = 0b10000010;
   /*
   OPTION_REG.RBPU   = 1;
   OPTION_REG.INTEDG = 0;
   OPTION_REG.T0CS   = 0;
   OPTION_REG.T0SE   = 0;
   OPTION_REG.PSA    = 0;
   OPTION_REG.PS2    = 0;
   OPTION_REG.PS1    = 1;
   OPTION_REG.PS0    = 0;
   */
   
   CMCON             = 0b00110011;
   /*
   CMCON.C2OUT       = 0;
   CMCON.C1OUT       = 0;
   CMCON.C2INV       = 1;
   CMCON.C1INV       = 1;
   CMCON.CIS         = 0;
   CMCON.CM2         = 0;
   CMCON.CM1         = 1;
   CMCON.CM0         = 1;
   */
   
   PIE1              = 0b01000010;
   /*
   PIE1.CMIE         = 1;
   PIE1.TMR2IE       = 1;
   */
   
   PR2               = 0xff;
   CCPR1L            = duty_max; //заполнение pwm

   CCP1CON           = 0b00111100;
   /*
   CCP1CON.CCP1X     = 1;
   CCP1CON.CCP1Y     = 1;
   CCP1CON.CCP1M3    = 1;
   CCP1CON.CCP1M2    = 1;
   CCP1CON.CCP1M1    = 0;
   CCP1CON.CCP1M0    = 0;
   */
   
   T2CON             = 0b00000100;
   /*
   T2CON.TMR2ON      = 1;
   T2CON.T2CKPS1     = 0;
   T2CON.T2CKPS0     = 0;
   */
   
   INTCON            = 0b11110000;
   /*
   INTCON.T0IE       = 1;
   INTCON.INTE       = 1;
   INTCON.PEIE       = 1;
   INTCON.GIE        = 1;
   */
   tmp = output[current_state];
   PORTB = tmp;

   while (1) {};

}
