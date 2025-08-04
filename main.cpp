#include "mbed.h"

I2C i2c(P0_5,P0_4);     // sda, scl
DigitalIn sw_l(P1_1);
DigitalIn sw_r(P1_2);
DigitalIn sw_u(P1_8);
DigitalIn sw_d(P1_9);

//lcd
const uint8_t contrast=0;  //lcd contrast
const uint8_t lcd_addr=0x7C;   //lcd i2c addr 0x7C
void lcd_init(uint8_t addr, uint8_t contrast);     //lcd init func
void char_disp(uint8_t addr, uint8_t position, char data);
void val_disp(uint8_t addr, uint8_t position, uint8_t digit,uint16_t val);

//AP33772S
const uint8_t ap_addr=0x52<<1;     //AP33772 address
const uint8_t PDREQ=0x31;
const uint8_t SRCPDO=0x20;
char s_buf[3];  //send buffer
char r_buf[26]; //receive buffer
uint8_t num, sel_pdo=1;    //number of all pdos, selected pdo
uint16_t pps_vol, max_cur;   //PPS voltage, PDO max current
uint8_t table[13][4];   //PDO table

int main(){
    sw_l.mode(PullUp);  //left sw
    sw_r.mode(PullUp);  //right sw
    sw_u.mode(PullUp);  //up sw
    sw_d.mode(PullUp);  //down sw

    i2c.frequency(100000);  //I2C clk 100kHz
    thread_sleep_for(100);  //wait for LCD power on
    lcd_init(lcd_addr, contrast);
    
    //pdo check
    thread_sleep_for(500);  //wait for AP33772S power on
    s_buf[0]=SRCPDO;
    i2c.write(ap_addr,s_buf,1);
    i2c.read(ap_addr|1,r_buf,26);
    for (uint8_t i=1;i<=7;++i){ //SPR PDO
        if((r_buf[2*i-1]>>6)==0)break;
        printf("PDO%d: EN=%x, CUR=%x, PC/VM=%x, VOL=%dmV\n\r",i, r_buf[2*i-1]>>6,(r_buf[2*i-1]>>2)&0b1111,(r_buf[2*i-1])&0b11,r_buf[2*i-2]*100);
        num++;
        table[num][0]=(uint8_t)(r_buf[2*i-2]/10);   //voltage
        table[num][1]=i;    //index
        table[num][2]=(r_buf[2*i-1]>>2)&0b1111; //current
        table[num][3]=r_buf[2*i-1]>>6;  //type. 2=Fixed, 3=APDO
    }
    for (uint8_t i=8;i<=13;++i){ //EPR PDO
        if((r_buf[2*i-1]>>6)==0)break;
        printf("PDO%d: EN=%x, CUR=%x, PC/VM=%x, VOL=%dmV\n\r",i, r_buf[2*i-1]>>6,(r_buf[2*i-1]>>2)&0b1111,(r_buf[2*i-1])&0b11,r_buf[2*i-2]*200);
        num++;
        table[num][0]=(uint8_t)(r_buf[2*i-2]/5);
        table[num][1]=i;
        table[num][2]=(r_buf[2*i-1]>>2)&0b1111;
        table[num][3]=r_buf[2*i-1]>>6;  //type. 2=Fixed, 3=APDO
    }

    while (true){
        val_disp(lcd_addr,0,2,table[sel_pdo][0]);
        char_disp(lcd_addr,2,'V');
        char_disp(lcd_addr,3,' ');
        val_disp(lcd_addr,4,1,sel_pdo);
        char_disp(lcd_addr,5,'/');
        val_disp(lcd_addr,6,1,num);
        char_disp(lcd_addr,7,' ');

        if(sw_u==0)sel_pdo=sel_pdo+1;    //pdo up
        if(sw_d==0)sel_pdo=sel_pdo-1;    //pdo down
        if(sel_pdo>num)sel_pdo=1;
        if(sel_pdo<1)sel_pdo=num;
        val_disp(lcd_addr,4,1,sel_pdo);
        val_disp(lcd_addr,0,2,table[sel_pdo][0]);
        if(table[sel_pdo][3]==3){
            char_disp(lcd_addr,0x40+0,'P');
            char_disp(lcd_addr,0x40+1,'P');
            char_disp(lcd_addr,0x40+2,'S');
        }else{
            char_disp(lcd_addr,0x40+0,' ');
            char_disp(lcd_addr,0x40+1,' ');
            char_disp(lcd_addr,0x40+2,' ');
        }
        thread_sleep_for(200);

        if(sw_r==0){
            if(table[sel_pdo][3]==3){  //PPS variable voltage request
                s_buf[0]=PDREQ;
                s_buf[1]=0x32;//5V req first
                s_buf[2]=0x0+((table[sel_pdo][1])<<4)+(table[sel_pdo][2]); //APDO index, max current
                i2c.write(ap_addr,s_buf,3);
                pps_vol=5000;   //mV unit. default
                val_disp(lcd_addr,0,2,pps_vol/1000);
                char_disp(lcd_addr,2,'.');
                val_disp(lcd_addr,3,1,(pps_vol%1000)/100);
                char_disp(lcd_addr,4,'V');
                char_disp(lcd_addr,5,' ');
                char_disp(lcd_addr,6,' ');
                char_disp(lcd_addr,7,' ');
                thread_sleep_for(200);
                while (true){
                    if((sw_u==0)&&(sw_d==0)) break;
                    if(sw_r==0)pps_vol=pps_vol+1000;    //1V up
                    if(sw_l==0)pps_vol=pps_vol-1000;    //1V down
                    if(sw_u==0)pps_vol=pps_vol+100;    //0.1V up
                    if(sw_d==0)pps_vol=pps_vol-100;    //1V up
                    if(pps_vol>=(table[sel_pdo][0]*1000))pps_vol=(table[sel_pdo][0]*1000);
                    if(pps_vol<=5000)pps_vol=5000;
                    s_buf[0]=PDREQ;
                    s_buf[1]=(char)(pps_vol/100);  //PPS Voltage req
                    s_buf[2]=0x0+((table[sel_pdo][1])<<4)+(table[sel_pdo][2]); //APDO index, max current
                    i2c.write(ap_addr,s_buf,3);
                    val_disp(lcd_addr,0,2,pps_vol/1000);
                    val_disp(lcd_addr,3,1,(pps_vol%1000)/100);
                    thread_sleep_for(200);
                }
            }else{  //fixed voltage request
                s_buf[0]=PDREQ;
                s_buf[1]=0x32;//no mean in fixed pdo.
                s_buf[2]=0x0+((table[sel_pdo][1])<<4)+(table[sel_pdo][2]); //fixed PDO index, max current
                i2c.write(ap_addr,s_buf,3);
                char_disp(lcd_addr,0+0x40,'O');
                char_disp(lcd_addr,1+0x40,'N');
                while (true){
                    if(sw_l==0) break;
                    thread_sleep_for(200);
                }
            }
        }
    }    
}
//disp char func
void char_disp(uint8_t addr, uint8_t position, char data){
    char buf[2];
    buf[0]=0x0;
    buf[1]=0x80+position;   //set cusor position (0x80 means cursor set cmd)
    i2c.write(addr,buf,2);
    buf[0]=0x40;            //write command
    buf[1]=data;
    i2c.write(addr,buf,2);
}

//disp val func
void val_disp(uint8_t addr, uint8_t position, uint8_t digit, uint16_t val){
    char buf[2],data[4];
    uint8_t i;
    buf[0]=0x0;
    buf[1]=0x80+position;       //set cusor position (0x80 means cursor set cmd)
    i2c.write(addr,buf,2);
    data[3]=0x30+val%10;        //1
    data[2]=0x30+(val/10)%10;   //10
    data[1]=0x30+(val/100)%10;  //100
    data[0]=0x30+(val/1000)%10; //1000
    buf[0]=0x40;                //write command
    for(i=0;i<digit;++i){
        if(i==0&&data[0]==0x30&&digit==4) buf[1]=0x20;
        else buf[1]=data[i+4-digit];
        i2c.write(addr,buf,2);
    }
}

//LCD init func
void lcd_init(uint8_t addr, uint8_t contrast){
    char lcd_data[2];
    lcd_data[0]=0x0;
    lcd_data[1]=0x38;
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x39;
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x14;
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x70|(contrast&0b1111);
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x56|((contrast&0b00110000)>>4);
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x6C;
    i2c.write(addr,lcd_data,2);
    thread_sleep_for(200);
    lcd_data[1]=0x38;
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x0C;
    i2c.write(addr,lcd_data,2);
    lcd_data[1]=0x01;
    i2c.write(addr,lcd_data,2);
}
