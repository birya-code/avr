/*
 * dynamic_ind+lm75.c
 *
 * Created: 19.03.2020 15:24:30
 * Author : BillyG
 */ 

//с общим катодом


#define F_CPU 4000000UL

#define F_I2C 100000UL //частота шины twi 100 кГц
#define TWBR_VALUE (((F_CPU)/(F_I2C)-16)/2)

#define Skip_Flag           0xff  // Значение пропуска, проверить что бы не совпадало с возможными данными
// Иначе изменить на неиспользуемое число

#define LM75AD_ADDRESS_READ      0b10010001
#define LM75AD_ADDRESS_WRITE     0b10010000
#define TEMP                     0x00 //адрес регистра Temp LM75
#define CONFIG                   0x01 //регистр настроек датчика температуры

#if ((TWBR_VALUE > 255) || (TWBR_VALUE == 0))//TWBR не должно превышать 255
#error "TWBR value is not correct"
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <math.h>
#include <avr/wdt.h>
#include <avr/sleep.h>


int category0=0, category1=0,category2=0,category3=0;//объявляем переменные для разрядов числа и обнуляем их
unsigned char i=1;//переменная для таймера
uint8_t H_temp, L_temp;
uint16_t full_temp;
int8_t mytemp,realtemp;
int8_t temp;


/*настройка портов*/
void init_ports(void){ 
	
	DDRB=0xff;//порт на выход(катоды семисегментника)
	PORTB=0xff;//положительный потенциал на выход(запираем катоды)
	
	DDRC|=(1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);//катоды семисегментника
	PORTC&=~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	DDRD|=(0<<PD2)|(1<<PD0);//кнопка для внешнего прерывания+led
	PORTD|=(1<<PD2);//подтяжка
	
}






/*
//массив с кодами цифр для семисегментника  с общим катодом цифры под криво разведенную плату
int digit[15]={
	0b00010100, //0
	0b01110111, //1
	0b01001100, //2
	0b01000110, //3
	0b00100111, //4
	0b10000110, //5
	0b10000100, //6
	0b01010111, //7
	0b00000100, //8
	0b00000110, //9
	0b11111111, // все сегменты погашены
	0b11101111,   //-
	0b11111011, //.
	0b10011100, //значок цельсия
	0b00001111 //значок градуса
};
*/




//массив с кодами цифр для семисегментника
int digit[15]={
	~0b11000000, //0
	~0b11111001, //1
	~0b10100100, //2
	~0b10110000, //3
	~0b10011001, //4
	~0b10010010, //5
	~0b10000010, //6
	~0b11111000, //7
	~0b10000000, //8
	~0b10010000, //9
	~0b11111111, // все сегменты погашены
	~0b10111111,//-
	~0b01111111,//. 
	~0b10011100//значок градуса
	
};




ISR (TIMER0_OVF_vect){                          //функция, которая будет выполняться по прерыванию в таймере0 при переполнении счетчика в таймере
	if (i==1){PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC2);PORTC&=~(1<<PC3);PORTB=digit[category3];if(category3==0){PORTB=digit[13];};}//зажигаем 1 цифру
	if (i==2){PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC3);PORTC&=~(1<<PC2);PORTB=digit[category2];}//зажигаем 2 цифру
	if(i==3){PORTC|=(1<<PC0)|(1<<PC3)|(1<<PC2);PORTC&=~(1<<PC1);PORTB=digit[category1];if(((H_temp&1<<7)!=0)&&category1==0){PORTB=digit[11];} else{if(category1==0){PORTB=digit[10];};};};
	if(i==4){PORTC|=(1<<PC3)|(1<<PC1)|(1<<PC2);PORTC&=~(1<<PC0);/*PORTB=digit[category0]*/;if(category0==0){PORTB=digit[10];};if(((H_temp&1<<7)!=0)){PORTB=digit[11];}; if(((H_temp&1<<7)!=0)&&category1==0){PORTB=digit[10];};};
	i++;
	if(i>4){i=1;};
}



//инициализация и настройка таймера0 для работы
void ini_timer0(){
	TCCR0|=(0<<CS02)|(1<<CS01)|(1<<CS00);//делитель 8:  2000000/8 = 250 кГц
	TCNT0=0; //обнуляем счетчик таймера0
	TIMSK|=(1<<TOIE0);//разрешаем прерывания для таймера0
	
};


void clear_display(void){
	PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);
}

/*функция разбивки числа на разряды*/
void number(int number){ 
	category0 = number/1000;//тысячи (остаток от деления убирает целое значение из получившегося дробного числа)
	category1 = number%1000/100;//сотни
	category2 = number%100/10;//десятки
	category3 = number%10;//единицы
	
}


//инициализация TWI
void TWI_Init(void)
{
	TWBR = TWBR_VALUE;//записываем в регистр данных вычисленное значение частоты шины twi
	TWSR = 0;// устанавливаем в нулевой и первый биты регистра TWSR нули для упрощения расчета частоты(см. выше)
}




//запускаем twi
void twi_start(void){
	/*формируем состояние СТАРТ*/
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);  // Сброс флага прерывания TWINT, установка СТАРТ, установка разрешения работы twi
	while(!(TWCR & (1<<TWINT)));             // Ждем пока будет свободна линия, если занята
	
}





//отправляем байт
void twi_sendbyte(uint8_t data){
	TWDR = data;                 // Передаем адрес с флагом записи 0
	TWCR = (1<<TWINT)|(1<<TWEN);             // Говорим, что мы хотим передать данные
	while(!(TWCR & (1<<TWINT)));            // Ждем пока будет свободна линия, если занята
	

}


//останавливаем twi
void twi_stop(void){
	
	TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);// Сброс флага прерывания TWINT, установка СТОП, установка разрешения работы twi
}


uint8_t lm75_get_byte(void){
	uint8_t data;
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
	while(!(TWCR & (1<<TWINT)));            // Ждем пока будет свободна линия, если занята
	data=TWDR;
	return data;
}

uint8_t lm75_get_last_byte(void){
	uint8_t data;
	TWCR = (1<<TWINT)|(1<<TWEN);
	TWCR&=~(1<<TWEA);
	while(!(TWCR & (1<<TWINT)));            // Ждем пока будет свободна линия, если занята
	data=TWDR;
	return data;
}

int8_t get_TEMP(void){
	
	twi_start();
	twi_sendbyte(LM75AD_ADDRESS_WRITE);
	twi_sendbyte(TEMP);
	twi_start();
	twi_sendbyte(LM75AD_ADDRESS_READ);
	H_temp=lm75_get_byte();
	L_temp=lm75_get_last_byte();
	twi_stop();
	full_temp=(H_temp<<8)|L_temp;
	mytemp=full_temp>>8;
	if((H_temp&1<<7)==0){realtemp=mytemp;}
	else{realtemp=(~mytemp+1);};
	return realtemp;
}


void lm75_shutdown(void){
	twi_start();
	twi_sendbyte(LM75AD_ADDRESS_WRITE);
	twi_sendbyte(CONFIG);
	twi_sendbyte(0x01);
	twi_stop();
}

void lm75_start(void){
	twi_start();
	twi_sendbyte(LM75AD_ADDRESS_WRITE);
	twi_sendbyte(CONFIG);
	twi_sendbyte(0x00);
	twi_stop();
}



//настройка внешних прерываний
void init_int0(void){
	GICR|=(1<<INT0);//разрешаем внешние прерывания на ноге int0
	MCUCR&=~((1<<ISC01)|(1<<ISC00));//условие срабатывания - низкий уровень на int0

}




ISR(INT0_vect){
	
//PORTD|=(1<<PD0);
}



int main(void)
{
	init_ports();
	ini_timer0();
	TWI_Init();
	init_int0();
set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sei();//разрешаем глобальные прерывания
	while (1)
	{
		clear_display();
		_delay_ms(500);
		lm75_start();//стартуем датчик
		PORTD|=(1<<PD0);
		_delay_ms(500);//дадаим время датчику для нового преобразования
		temp=get_TEMP();//считываем температуру
		number(temp*10);//после включения показываем температуру 5с
		_delay_ms(5000);//показываем температурe  5c
       clear_display();
       PORTD&=~(1<<PD0);
	   lm75_shutdown();
	  sleep_enable(); // разрешаем сон
	   sleep_cpu(); // засыпаем
	   
	
	
	
		
		
	}
}

