// ID3 ID2 ID1 ID0 EXT RTR DTA0 DTA1 DTA2 DTA3 DTA4 DTA5 DTA6 DTA7

#include <Serial_CAN_Module.h>
//#include <SoftwareSerial.h>
//#include <HardwareSerial.h>

#include <wiringPi.h>
#include <wiringSerial.h>   // wiringPi Serial 


// See mapping to https://www.electronicwings.com/raspberry-pi/raspberry-pi-uart-communication-using-python-and-c
Serial_CAN::Serial_CAN() {

}

Serial_CAN::~Serial_CAN() {
    printf("serial can destructor\n");
    serialFlush(_fd);
    printf("Closing serial port\n"); 
    serialClose(_fd);
}

void Serial_CAN::begin(unsigned long baud)
{



    //canSerial = 
    _fd = serialOpen(SERIAL_PORT, baud); 

    if (_fd < 0) {
        printf("Port open failure!\n");
    }

    if (wiringPiSetup() == -1) {
        printf("wiring pi failure\n");
    }


    serialFlush(_fd);
}


// TODO -- If need add a "Reset" Function and see how long it takes 
// If < 400 us or so, then reseting on failure would be viable 

bool Serial_CAN::reset() 
{
    printf("Resetting serial\n");
    unsigned long tic = micros();
    bool retval = true;
    serialClose(_fd); 
    _fd = serialOpen(SERIAL_PORT, SERIAL_RATE_115200); 

    if (_fd < 0) {
        printf("Port open failure!\n");
        retval = false;
    }

    serialFlush(_fd);

    // Add print timing 
    unsigned long toc = micros();
    printf("Serial Reset time %lu\n", (toc -  tic));
}






/*


void Serial_CAN::begin(int can_tx, int can_rx, unsigned long baud)
{
    //softwareSerial = new SoftwareSerial(can_tx, can_rx);
    //softwareSerial->begin(baud);
    //canSerial = softwareSerial;
    

}



void Serial_CAN::begin(SoftwareSerial &serial, unsigned long baud)
{
    serial.begin(baud);
    softwareSerial = &serial;
    canSerial = &serial;
}

void Serial_CAN::begin(HardwareSerial &serial, unsigned long baud)
{
    serial.begin(baud);
    hardwareSerial = &serial;
    canSerial = &serial;
}
*/



unsigned char Serial_CAN::sendMsgBuf(unsigned long id, uchar ext, uchar rtrBit, uchar len, const uchar *buf)
{
    unsigned char dta[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    
    dta[0] = id>>24;        // id3
    dta[1] = id>>16&0xff;   // id2
    dta[2] = id>>8&0xff;    // id1
    dta[3] = id&0xff;       // id0
    
    dta[4] = ext;
    dta[5] = rtrBit;
    
    for(int i=0; i<len; i++)
    {
        dta[6+i] = buf[i];
    }
    
    /*
    for(int i=0; i<14; i++)
    {
        canSerial->write(dta[i]);
    }
    */
    write(_fd, dta, 14); // EREZ
}


// 0: no data
// 1: get data
unsigned char Serial_CAN::recv(unsigned long *id, uchar *buf)
{

    /*
    if(canSerial->available())
    {
        unsigned long timer_s = millis();
        
        int len = 0;
        uchar dta[20];
        
        while(1)
        {
            //while(canSerial->available())
            while(serialDataAvail(_fd)) // EREZ
            {
                dta[len++] = canSerial->read();
                if(len == 12)
                    break;
 
            	if((millis()-timer_s) > 10)
                {
                    canSerial->flush();
                    return 0; // Reading 12 bytes should be faster than 10ms, abort if it takes longer, we loose the partial message in this case
                }
            }
            
            if(len == 12) // Just to be sure, must be 12 here
            {
                unsigned long __id = 0;
                
                for(int i=0; i<4; i++) // Store the id of the sender
                {
                    __id <<= 8;
                    __id += dta[i];
                }
                
                *id = __id;
                
                for(int i=0; i<8; i++) // Store the message in the buffer
                {
                    buf[i] = dta[i+4];
                }
                return 1;
            }
            
            if((millis()-timer_s) > 10)
            {
                canSerial->flush();
                return 0; // Reading 12 bytes should be faster than 10ms, abort if it takes longer, we loose the partial message in this case
            }
            
        }
    }
    */

    int nbytes = serialDataAvail(_fd);

    if (nbytes >= 12)  { // EREZ 

        if (nbytes > 12) {
            printf("%d bytes AVAILABLE\n", nbytes);
        }
        // read 12 byes
        uchar dta[12];
        read(_fd, dta, 12); 
        unsigned long __id = 0;
                
        for(int i=0; i<4; i++) // Store the id of the sender
        {
            __id <<= 8;
            __id += dta[i];
        }
        
        *id = __id;
        
        for(int i=0; i<8; i++) // Store the message in the buffer
        {
            buf[i] = dta[i+4];
        }
        return 1;
    } else if (nbytes > 0) {
        printf("ONLY %d bytes AVAILABLE\n", nbytes);    
        return 0;
    } else {
        printf("NO SERIAL AVAILABLE\n");
        return 0;
    }
}


// need a max time here otherwise wont know if error 
unsigned char Serial_CAN::block_recv(unsigned long *id, uchar *buf, int delay_ms)
{
    // TODO -- can add a debug response variable -- true if we had to wait 


    unsigned long timer_s = millis();

    // Wait for enough data to become available 
    int nbytes = serialDataAvail(_fd);
    unsigned char retval = 0;

    while ((nbytes < 12) && (millis() - timer_s < delay_ms)) { 
        // do nothing 
        nbytes = serialDataAvail(_fd); 
        //printf("AA: nbytes %d\n", nbytes);
    }

    while(nbytes >= 12) { 
        retval = recv(id, buf); 
        nbytes = serialDataAvail(_fd); 
        //printf("BB: nbytes %d\n", nbytes);

    }
    return retval;
}
    // Read until less than twelve 

// send command and then checks response 

unsigned char Serial_CAN::cmdOk(char *cmd)
{
    
    unsigned long timer_s = millis();
    unsigned char len = 0;

    // All commands EXCEPT +++ should end in '\n'

    //canSerial->println(cmd);
    serialPrintf(_fd, cmd);
 
    while(1)
    {
        if(millis()-timer_s > 500)
        {
            return 0;
        }
        
        //while(canSerial->available())
        while(serialDataAvail(_fd))
        {

            //str_tmp[len++] = canSerial->read();
            str_tmp[len++] = serialGetchar(_fd); 

            timer_s = millis();
        }

        if(len >= 4 && str_tmp[len-1] == '\n' && str_tmp[len-2] == '\r' && str_tmp[len-3] == 'K' && str_tmp[len-4] == 'O')
        {
            clear();
            return 1;        
        }
        
    }
}

/*
value	    01	02	03	04	05	    06	07	08	09	10	    11	12	13	14	15	16	17	18
rate(kb/s)	5	10	20	25	31.2	33	40	50	80	83.3	95	100	125	200	250	500	666	1000
*/
unsigned char Serial_CAN::canRate(unsigned char rate)
{
    enterSettingMode();
    if(rate < 10)
        sprintf(str_tmp, "AT+C=0%d\r\n", rate);
    else 
        sprintf(str_tmp, "AT+C=%d\r\n", rate);
    
    int ret = cmdOk(str_tmp);
    exitSettingMode();
    return ret;
}

/*
value	        0	    1	    2	    3   	4
baud rate(b/s)	9600	19200	38400	/	115200
*/

unsigned char Serial_CAN::baudRate(unsigned char rate)
{
    unsigned long baud[5] = {9600, 19200, 38400, 9600, 115200};
    int baudNow = 0;
    
    if(rate == 3)return 0;


    // Loops through all possible bauds and checks for response 
    // This loop figures out what baud its at right now 
    for(int i=0; i<5; i++)
    {
        //printf("opening with new baud...\n");
        selfBaudRate(baud[i]);
        //canSerial->print("+++");
        //serialPrintf(_fd, "+++"); // basically, enter setting modd 

        enterSettingMode();


        //delay(100);
        usleep(100000);
        if(cmdOk("AT\r\n"))
        {
            //Serial.print("SERIAL BAUD RATE IS: ");
            //Serial.println(baud[i]);
            printf("Got baud response at: %d\n", baud[i]);
            baudNow = i;
            break;     
        }
    }


    // AT+S is the set baud success 
    // Thi sets the baud using whatever baud its currently responsing too
    sprintf(str_tmp, "AT+S=%d\r\n", rate);
    cmdOk(str_tmp);



    
    selfBaudRate(baud[rate]);
    
    int ret = cmdOk("AT\r\n");
    
    if(ret)
    {
        //Serial.print("Serial baudrate set to ");
        //Serial.println(baud[rate]);
        printf("Serial baudrate set to %d\n", baud[rate]);
        
    } else {
        printf("Serial baud error\n");
    }
    
    exitSettingMode();
    return ret;
}


void Serial_CAN::selfBaudRate(unsigned long baud)
{
    /*
    if(softwareSerial)
    {
        softwareSerial->begin(baud);
    }
    else if(hardwareSerial)
    {
        hardwareSerial->begin(baud);
    }
    */
    serialClose(_fd);
    _fd = serialOpen(SERIAL_PORT, baud); 

    if (_fd < 0) {
        printf("Port open failure!\n");
    }

    serialFlush(_fd);

}
 
// TODO -- make it retrn something if there was anything 
// available to clear 

// add a flush command 

void Serial_CAN::clear()
{
    unsigned long timer_s = millis();
    while(1)
    {
        if(millis()-timer_s > 50)return;
        //while(canSerial->available())
        while(serialDataAvail(_fd))
        {
            //canSerial->read();
            //uchar* tmp;
            //read(_fd, tmp, 1);
            serialGetchar(_fd);
            timer_s = millis();
        }
    }
}

unsigned char Serial_CAN::enterSettingMode()
{
    //canSerial->print("+++");
    serialPrintf(_fd, "+++"); // EREZ 



    

    // CLEAR TAKES 50 ms anyway 
    clear();



    return 1;
}


// Should return it to noromal mode 
unsigned char Serial_CAN::exitSettingMode()
{
    clear();
    int ret = cmdOk((char*)"AT+Q\r\n");
    clear();

    if (ret==0) {
        printf("Failure exiting settings\n"); 
    }

    return ret;
}

void make8zerochar(int n, char *str, unsigned long num)
{
    for(int i=0; i<n; i++)
    {
        str[n-1-i] = num%0x10;
        if(str[n-1-i]<10)str[n-1-i]+='0';
        else str[n-1-i] = str[n-1-i]-10+'A';
        num >>= 4;
    }
    str[n] = '\0';    
}

/*
+++	                    Switch from Normal mode to Config mode
AT+S=[value]	        Set serial baud rate
AT+C=[value]	        Set CAN Bus baud rate
AT+M=[N][EXT][value]    Set mask,AT+M=[1][0][000003DF]
AT+F=[N][EXT][value]    Set filter,AT+F=[1][0][000003DF]
AT+Q	            Switch to Normal Mode
*/
unsigned char Serial_CAN::setMask(unsigned long *dta)
{
    enterSettingMode();
    char __str[10];
    
    
    for(int i=0; i<2; i++)
    {
        make8zerochar(8, __str, dta[1+2*i]);
        //Serial.println(__str);
        printf(__str); printf("\n"); 
        sprintf(str_tmp, "AT+M=[%d][%d][", i, dta[2*i]);
        for(int i=0; i<8; i++)
        {
            str_tmp[12+i] = __str[i];
        }
        str_tmp[20] = ']';
        str_tmp[21] = '\r';
        str_tmp[22] = '\n';
        str_tmp[23] = '\0';
        
        //Serial.println(str_tmp);
        printf(str_tmp); printf("\n");
        
        if(!cmdOk(str_tmp))
        {
            //Serial.print("mask fail - ");
            //Serial.println(i);
            printf("mask fail - %d\n", i);
            exitSettingMode();
            return 0;
        }
        clear();
        //delay(10);
        usleep(10000);
        //
    }
    exitSettingMode();
    return 1;

}

unsigned char Serial_CAN::setFilt(unsigned long *dta)
{
    enterSettingMode();
    
    char __str[10];
    
    for(int i=0; i<6; i++)
    {
        make8zerochar(8, __str, dta[1+2*i]);
        //Serial.println(__str);
        sprintf(str_tmp, "AT+F=[%d][%d][", i, dta[2*i]);
        for(int i=0; i<8; i++)
        {
            str_tmp[12+i] = __str[i];
        }
        str_tmp[20] = ']';
        str_tmp[21] = '\r';
        str_tmp[22] = '\n';
        str_tmp[23] = '\0';
        
        //Serial.println(str_tmp);
        
        clear();
        if(!cmdOk(str_tmp))
        {
            //Serial.print("filt fail at - ");
            //Serial.println(i);
            exitSettingMode();
            return 0;
        }
        clear();
        //delay(10);
        usleep(10000);
        //
    }
    exitSettingMode();
    return 1;
}

/*
value	        0	    1	    2	    3   	4
baud rate(b/s)	9600	19200	38400	57600	115200
*/

/*
unsigned char Serial_CAN::factorySetting()
{
    // check baudrate
    unsigned long baud[5] = {9600, 19200, 38400, 57600, 115200};
    
    for(int i=0; i<5; i++)
    {
        selfBaudRate(baud[i]);
        canSerial->print("+++");
        delay(100);
        
        if(cmdOk("AT\r\n"))
        {
            Serial.print("SERIAL BAUD RATE IS: ");
            Serial.println(baud[i]);
            baudRate(0);                // set serial baudrate to 9600
            Serial.println("SET SERIAL BAUD RATE TO: 9600 OK");
            selfBaudRate(9600);
            break;            
        }
    }
    
    if(canRate(CAN_RATE_500))
    {
        Serial.println("SET CAN BUS BAUD RATE TO 500Kb/s OK");
    }
    else
    {
        Serial.println("SET CAN BUS BAUD RATE TO 500Kb/s FAIL");
        return 0;
    }
    
    unsigned long mask[4] = {0, 0, 0, 0,};
    unsigned long filt[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
    
    if(setFilt(filt))
    {
        Serial.println("FACTORY SETTING FILTS OK");
    }
    else 
    {
        Serial.println("FACTORY SETTING FILTS FAIL");
        return 0;        
    }
    
    if(setMask(mask))
    {
        Serial.println("FACTORY SETTING MASKS OK");
    }
    else
    {
        Serial.println("FACTORY SETTING MASKS FAIL");
        return 0;
    }
    
    return 1;
}
*/


/*
void Serial_CAN::debugMode()
{
    while(Serial.available())
    {
        canSerial->write(Serial.read());
    }
    
    while(canSerial->available())
    {
        Serial.write(canSerial->read());
    }
}
*/

// END FILE
