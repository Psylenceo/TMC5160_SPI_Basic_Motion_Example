/******************************************************
    This example code shows all the options for a TMC5160 to be controlled from only SPI.
    The code will be broken up just a little bit to show the various registers that need to
    be set to get each function running.

    Currently this example works best on teemuatluts TMC stepper library version 4.5 and newer.
    
    Versions 4.3 and olderare missing some of the functions do not work or values are sent to wrong registers.
    
    For version details see teemuatluts github page for info..
    https://github.com/teemuatlut/TMCStepper
    
    Code written by: Psylenceo    
    Also Authour tends to use {} as code folding points.

    I have included the option to use an arduino uno or nano, but the code may become larger than the 328P on the nano or uno can handle.
    If that is the case use a more powerful processor, comment out some of the dialog options, or if I can figure it out I'll block off sections
    with a chip identifier if statement so that at compile time it will only compile what is needed and can fit on the 328P.
 *  *********************************************************************/
#include <Arduino.h>                          //Include the arduino header for use within platformIO
#include <TMCStepper.h>                       //header for tmcstepper library
#include <TMCStepper_UTILITY.h>               //header for thcstepper utility library

#define TMC5160_FULL_SPI_BASIC_MOTION_Example_VERSION 0x000100  //v0.1.0

/****************************************************
   This code uses the arduino hardware SPI, but software SPI
   can be used, I just have not set it up.
 *****************************************************/
/* If using arduino mega*/
#define sck      52                                    //SPI clock
#define mosi     51                                    //master transmit out slave receive input
#define miso     50                                    //master receive input slave transmit out
#define ss       53                                    //chip select

/*if using arduino uno, or nano*/
/*#define sck      13                                  //SPI clock
  #define mosi     11                                  //master transmit out slave receive input
  #define miso     12                                  //master receive input slave transmit out
  #define ss       10                                  //chip select
*/

#define drv_en 7                                       //pin 7 of the arduino will be used for driver enable and disable can be a pin of your choice

#define sense_resistor .075                            //Change this to the value of your sense resistor

#define supply_voltage 24                              //change this to the voltage you are trying to run the driver chip and motor at

/********************************************************
   Example motor -> Kysan 1040118 17HD-B8X300-0.4A  -> http://kysanelectronics.com/Products/datasheet_display.php?recordID=8585
   Operating voltage-> 12 volts
   operating current -> .4A   (400mA)
   coil resistance ->  30 ohms
   coil inductance ->  37 mH
   Holding torque -> 26 Ncm (260 mNm)
   Rotor torque -> 35 gcm^2
   Degrees ->1.8
   All of that info was from the data sheet and some of it is shown on the product page
 **********************************************************/
#define motor_voltage 12                              //Motor operating voltage shown on datasheet. Change tp match your motor.
#define motor_milliamps 400                           //Milliamps specified on motor datasheet. Change to match your motor.
#define motor_resistance 30                           //Motor coil resistance from datasheet. Change to match your motor.
#define motor_hold_torque 260                         //Motor holding torque from datasheet. Change to match your motor. May need to calculate
#define motor_step_degrees 1.8                        //angle of rotation per step of motor
#define motor_us_counts (360/motor_step_degrees)       //number of full steps per full rotation of motor. 360 / degrees = full step count

/****************************************************
   Now we need to define some of the base setting that
   we'll be using with the driver chip.

   This example will be using the drivers internal clock which runs at 12MHz.

   Also in the drivers data sheet, in the stealth chop section
   it talks about pwm freq, clock frequency, and good ranges of operation.
   to try and stay within 20kHz-50kHz range, with the the optimum pwm freq
   being in the green section for a given clock frequency and selected PWM frequency.

   Since this example is using the internall 12MHz clock and
   we want the widest adjustment range, we will be using
   pwm_freq = 2, for starting frequency of 35.1kHz.
 ***********************************************************/
#define drv_clock 12000000                          //using internal clock, tie the clk pin low. If using a different freq, change this value to match
#define drv_chop_freq 35100                         //drivers chop frequency set by pwm_freq and based on clk frequency. Change to match.
#define drv_decay_percent .7                        //percentage (as a decimal) of chopper standstill cycle time for lower power dissipation and upper frequency limit
#define drv_microstep_res 256                       //number of micro steps per full step. 

/************************************************************
   Now we need to calculate some important values for inital register settings in the driver. If you want to adjust any
   of the register settings after initialization. You can change them below in the setup section.

   First we calculate the nominal amperage of the motor, this is important if we are using a voltage other than what the motor
   is rated for:
   For example 12 volt @ .4 amps = 4.8 watts if we use 24 volts to power the motor
   then 4.8 watts / 24 volts = .2 amps.

   This is then used to set the Irun and Ihold registers along with tuning of the chopper modes.

   We also need to calculate the initial PWM offset.
   Finally we calculate the drivers PWM off time.
 ***********************************************************/

#define nominal_amps (((motor_milliamps * motor_voltage) / supply_voltage) * 1.0)                                //calculate the voltage based curent
#define driv_toff ((((100000 / drv_chop_freq) * drv_decay_percent * .5) * drv_clock - 12) / 3200000)             //calculate the toff of the motor, currently does not calculate value

/***********************************************************
   Using the example motor, this gives us results of:

   Nominal amps -> 200mA
   Toff -> 3.36                       (so toff should be set to 3 or 4, may need some testing)

   Now that we have our initializers calculated we need to tell the arduino what driver we are using and and give the register points
   a prefix. This is needed, but is useful when multiple motors are used with a single CPU.
 **********************************************************/

TMC5160Stepper driver = TMC5160Stepper(ss, sense_resistor); //we are also telling the libray what pin is chip select and what the sense resistor value is.

void setup() {
  /* start up SPI, configure SPI, and axis IO interfacing*/{
    SPI.begin();                            //start spi
    pinMode(sck, OUTPUT);                   //set sck pin as output for spi clock
    pinMode(ss, OUTPUT);                    //set ss pin as an out put for chip select
    pinMode(drv_en, OUTPUT);                //set drv enable pin as out put
  }

  digitalWrite(drv_en, LOW);                //enable the driver so that we can send the initial register values

  /*Initial settings for basic SPI command stepper drive no other functions enabled*/ {
    driver.begin();                         // start the tmc library

    /* base GCONF settings for bare stepper driver operation*/    {
      driver.recalibrate(0);                //do not recalibrate the z axis
      driver.faststandstill(0);             //fast stand still at 65ms
      driver.en_pwm_mode(0);                //no silent step
      driver.multistep_filt(0);             //normal multistep filtering
      driver.shaft(0);                      //motor direction cw
      driver.small_hysteresis(0);           //step hysteresis set 1/16
      driver.stop_enable(0);                //no stop motion inputs
      driver.direct_mode(0);                //normal driver operation
    }

    /* Set operation current limits */
    driver.rms_current(nominal_amps, 1);    //set Irun and Ihold for the drive

    /* short circuit monitoring */    {
      driver.diss2vs(0);                    //driver monitors for short to supply
      driver.s2vs_level(6);                 //lower values set drive to be very sensitive to low side voltage swings
      driver.diss2g(0);                     //driver to monitor for short to ground
      driver.s2g_level(6);                  //lower values set drive to be very sensitive to high side voltage swings
    }

    /* minimum settings to to get a motor moving using SPI commands */{
      driver.tbl(2);                          //set blanking time to 24
      driver.toff(driv_toff);                 //pwm off time factor
      driver.pwm_freq(1);                     //pwm at 35.1kHz
    }
  }

  /**********************************************************
   * The last steps before actually moving the motor is setting what ramp mode to use
   * and motion values. ( accel / deccel / velocities)
   *************************************************************/
  /* Ramp mode (default)*/{
    driver.RAMPMODE(0);             //set ramp mode to positioning
    driver.VSTOP(10);              //set stop velocity to 10 steps/sec
    driver.VSTART(0);             //set start velocity to 10 steps/sec

    driver.V1(600000);               //midpoint velocity to  steps/sec 
    driver.VMAX(838809);             //max velocity to  steps/sec 

    driver.A1(1);               //initial accel at  steps/sec2
    driver.AMAX(100);             //max accel at  steps/sec2 

    driver.DMAX(500);             //max deccel  steps/sec2 
    driver.D1(32000);               //mid deccel  steps/sec2 
  }

  /* Reseting drive faults and re-enabling drive */ {
    digitalWrite(drv_en, HIGH);             //disable drive to clear any start up faults
    delay(1000);                            //give the drive some time to clear faults
    digitalWrite(drv_en, LOW);              //re-enable drive, to start loading in parameters
    driver.GSTAT(7);                        //clear gstat faults
  }


}

void loop() {
  /*Now lets start the first actual move to see if everything worked, and to hear what the stepper sounds like.*/
    if (driver.position_reached() == 1) driver.XTARGET(250000);     //verify motor is at starting position, then move 250,000 counts in a positive direction
    while (driver.position_reached() == 0);                         //while in motion do nothing. This prevents the code from missing actions

    if (driver.position_reached() == 1) driver.XTARGET(0);          //verify motor is at position, then move motor back to starting position
    while (driver.position_reached() == 0);                         //while in motion do nothing. This prevents the code from missing actions
    
} //end of loop
//end of loop

//end of program