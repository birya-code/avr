/*
 * dynamic_ind+lm75.c
 *
 * Created: 19.03.2020 15:24:30
 * Author : BillyG
 */ 

//� ����� �������


#define F_CPU 4000000UL

#define F_I2C 100000UL //������� ���� twi 100 ���
#define TWBR_VALUE (((F_CPU)/(F_I2C)-16)/2)

#define Skip_Flag           0xff  // �������� ��������, ��������� ��� �� �� ��������� � ���������� �������
// ����� �������� �� �������������� �����

#define LM75AD_ADDRESS_READ      0b10010001
#define LM75AD_ADDRESS_WRITE     0b10010000
#define TEMP                     0x00 //����� �������� Temp LM75
#define CONFIG                   0x01 //������� �������� ������� �����������

#if ((TWBR_VALUE > 255) || (TWBR_VALUE == 0))//TWBR �� ������ ��������� 255
#error "TWBR value is not correct"
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <math.h>
#include <avr/wdt.h>
#include <avr/sleep.h>


int category0=0, category1=0,category2=0,category3=0;//��������� ���������� ��� �������� ����� � �������� ��
unsigned char i=1;//���������� ��� �������
uint8_t H_temp, L_temp;
uint16_t full_temp;
int8_t mytemp,realtemp;
int8_t temp;


/*��������� ������*/
void init_ports(void){ 
	
	DDRB=0xff;//���� �� �����(������ ���������������)
	PORTB=0xff;//������������� ��������� �� �����(�������� ������)
	
	DDRC|=(1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);//������ ���������������
	PORTC&=~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	DDRD|=(0<<PD2)|(1<<PD0);//������ ��� �������� ����������+led
	PORTD|=(1<<PD2);//��������
	
}






/*
//������ � ������ ���� ��� ���������������  � ����� ������� ����� ��� ����� ����������� �����
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
	0b11111111, // ��� �������� ��������
	0b11101111,   //-
	0b11111011, //.
	0b10011100, //������ �������
	0b00001111 //������ �������
};
*/




//������ � ������ ���� ��� ���������������
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
	~0b11111111, // ��� �������� ��������
	~0b10111111,//-
	~0b01111111,//. 
	~0b10011100//������ �������
	
};




ISR (TIMER0_OVF_vect){                          //�������, ������� ����� ����������� �� ���������� � �������0 ��� ������������ �������� � �������
	if (i==1){PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC2);PORTC&=~(1<<PC3);PORTB=digit[category3];if(category3==0){PORTB=digit[13];};}//�������� 1 �����
	if (i==2){PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC3);PORTC&=~(1<<PC2);PORTB=digit[category2];}//�������� 2 �����
	if(i==3){PORTC|=(1<<PC0)|(1<<PC3)|(1<<PC2);PORTC&=~(1<<PC1);PORTB=digit[category1];if(((H_temp&1<<7)!=0)&&category1==0){PORTB=digit[11];} else{if(category1==0){PORTB=digit[10];};};};
	if(i==4){PORTC|=(1<<PC3)|(1<<PC1)|(1<<PC2);PORTC&=~(1<<PC0);/*PORTB=digit[category0]*/;if(category0==0){PORTB=digit[10];};if(((H_temp&1<<7)!=0)){PORTB=digit[11];}; if(((H_temp&1<<7)!=0)&&category1==0){PORTB=digit[10];};};
	i++;
	if(i>4){i=1;};
}



//������������� � ��������� �������0 ��� ������
void ini_timer0(){
	TCCR0|=(0<<CS02)|(1<<CS01)|(1<<CS00);//�������� 8:  2000000/8 = 250 ���
	TCNT0=0; //�������� ������� �������0
	TIMSK|=(1<<TOIE0);//��������� ���������� ��� �������0
	
};


void clear_display(void){
	PORTC|=(1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);
}

/*������� �������� ����� �� �������*/
void number(int number){ 
	category0 = number/1000;//������ (������� �� ������� ������� ����� �������� �� ������������� �������� �����)
	category1 = number%1000/100;//�����
	category2 = number%100/10;//�������
	category3 = number%10;//�������
	
}


//������������� TWI
void TWI_Init(void)
{
	TWBR = TWBR_VALUE;//���������� � ������� ������ ����������� �������� ������� ���� twi
	TWSR = 0;// ������������� � ������� � ������ ���� �������� TWSR ���� ��� ��������� ������� �������(��. ����)
}




//��������� twi
void twi_start(void){
	/*��������� ��������� �����*/
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);  // ����� ����� ���������� TWINT, ��������� �����, ��������� ���������� ������ twi
	while(!(TWCR & (1<<TWINT)));             // ���� ���� ����� �������� �����, ���� ������
	
}





//���������� ����
void twi_sendbyte(uint8_t data){
	TWDR = data;                 // �������� ����� � ������ ������ 0
	TWCR = (1<<TWINT)|(1<<TWEN);             // �������, ��� �� ����� �������� ������
	while(!(TWCR & (1<<TWINT)));            // ���� ���� ����� �������� �����, ���� ������
	

}


//������������� twi
void twi_stop(void){
	
	TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);// ����� ����� ���������� TWINT, ��������� ����, ��������� ���������� ������ twi
}


uint8_t lm75_get_byte(void){
	uint8_t data;
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
	while(!(TWCR & (1<<TWINT)));            // ���� ���� ����� �������� �����, ���� ������
	data=TWDR;
	return data;
}

uint8_t lm75_get_last_byte(void){
	uint8_t data;
	TWCR = (1<<TWINT)|(1<<TWEN);
	TWCR&=~(1<<TWEA);
	while(!(TWCR & (1<<TWINT)));            // ���� ���� ����� �������� �����, ���� ������
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



//��������� ������� ����������
void init_int0(void){
	GICR|=(1<<INT0);//��������� ������� ���������� �� ���� int0
	MCUCR&=~((1<<ISC01)|(1<<ISC00));//������� ������������ - ������ ������� �� int0

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
	sei();//��������� ���������� ����������
	while (1)
	{
		clear_display();
		_delay_ms(500);
		lm75_start();//�������� ������
		PORTD|=(1<<PD0);
		_delay_ms(500);//������ ����� ������� ��� ������ ��������������
		temp=get_TEMP();//��������� �����������
		number(temp*10);//����� ��������� ���������� ����������� 5�
		_delay_ms(5000);//���������� ����������e  5c
       clear_display();
       PORTD&=~(1<<PD0);
	   lm75_shutdown();
	  sleep_enable(); // ��������� ���
	   sleep_cpu(); // ��������
	   
	
	
	
		
		
	}
}

