#include "DS18b20.h"
#include "misc.h"

char ds18b20_init()   //初始化  
{  
    u8 checkState=0;  
    DQ=1;             //总线初始状态  
    delayXus(20);  
    DQ=0;             //mcu先将总线拉低  
    delayXus(530);     //延时530us,要求480us~960us的低电平信号  
                      //当ic接受到此复位信号后会回发一个存在信号  
                      //mcu若要接收此存在信号则先要释放总线,让ic控制该总线  
                      //当mcu释放总线后的15~60us之后,ic才向总线发一个低电平信号  
                      //该信号存在时间为60~240us  
    DQ=1;             //mcu释放总线  
    delayXus(40);     //mcu释放15~60us以上,(8+6*10)*1.085=73us,  
                      //这时DS18B20已经拉低信号，大约60~240us应答保持时间,  
    checkState=DQ;    //在这段60~240us时间内,mcu采样是否有器件响应,0表示有响应  
    delayXus(464);     //延时464us,加上之前的73us,共537us  
                      //虽然ic在拉低电平60~240us之后,会释放总线,但整个时间至少480us  
                      //故我们共用时537us,这样是为了不影响后续的操作                      
    if(checkState)    //checkstate为0说明有器件响应,为1无器件响应  
    {  
        return 0;  
    }  
    return 1;      //初始化成功  
}

void ds18b20_writeByte(u8 dat)  //mcu向ic写一个字节  
{  
    u8 i;  
    u8 tmep=dat;  
    for(i=0;i<8;i++)  
    {  
        DQ=0;           //产生读写时序的起始信号  
        delayXus(1);        //要求至少1us的延时  
        DQ=dat & 0x01; //对总线赋值,从最低位开始写起  
        delayXus(74);//延时74us,写0在60~120us之间释放,写1的话大于60us均可释放  
        DQ=1;          //释放总线,为下一次mcu送数据做准备,         
        dat>>=1;     //有效数据移动到最低位,2次写数据间隙至少需1us  
    }  
}  


unsigned char ds18b20_readByte()    //mcu读一个字节  
{  
    u8 i,value=0;  
    for(i=0;i<8;i++)  
    {  
        DQ=0;                       //起始信号  
        value>>=1;                  //顺便延时3~4个机器周期  
        DQ=1;                       //mcu释放总线  
        delayXus(3);    //再延时3.3us   
        if(DQ)            
        {  
            value|=0x80;//保存高电平数据,低电平的话不用保存,移位后默认是0  
        }   
        delayXus(60); //延时60.76us    
    }  
    return value;  
} 

u16 ds18b20_readTemperaData()  //读取温度值  
{  
    u16 temp=0;  
    if(ds18b20_init())  
    {  
        ds18b20_writeByte(0xcc);      //写指令:跳过rom检测  
        ds18b20_writeByte(0x44);     //写指令:温度转换  
        //delayms(750);// 转换延时需要750ms以上,我们不等它  
        //首次转换未完成时,得到的初始化数据是85度,处理一下就可以了  
        //温度转换电路是硬件独立的,不会阻塞初始化功能   
        if(ds18b20_init())  
        {  
            ds18b20_writeByte(0xcc);  //写指令:跳过检测rom  
            ds18b20_writeByte(0xbe);  //写指令:读取温度值  
            temp=ds18b20_readByte();  //先读低8位数据  
            temp|=(u16)ds18b20_readByte()<<8; //再读高8位数据,然后合并  
            temp&= 0x0FFF;  //高4位数据反正没用上,我们用来存放错误码  
        }  
        else  
        {  
            //led5=0;      //调试代码  
            temp=0x2000; //错误码,初始化失败  
        }  
    }  
    else  
    {  
        //led6=0;         //调试代码  
        temp=0x1000;    //错误码,初始化失败,可能器件损坏  
    }  
    return temp;  
}  

char read_temp(short temp, float *d_temp) {
	if(0x0550==temp)      //如果初始化温度数据是85度的话  
    {  
        return 0;           //亮灯报警,调试   
    }  
  
    if(1==(temp&0x0800))  //检测第11位是否为1,为1是负温度  
    {  
        temp &= 0x07ff;     //只取第0~10共11个位  
        temp = (~temp+1) & 0x07ff;//将补码还原
		temp = -temp;  
    }  
    
    *d_temp = ((float)temp) * .0625;   //精度的1000倍,我们将小数点另外叠加显示
	return 1; 
}