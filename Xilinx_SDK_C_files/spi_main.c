/******************************************************************************/
/* ECE - 315 	: WINTER 2021
 * Created on 	: 07 August, 2021
 *
 * Created by	: Shyama M. Gandhi, Mazen Elbaz
 * Modified by	: Shyama M. Gandhi, Winter 2023
 *
 * LAB 3: Implementation of SPI in Zynq-7000
 *------------------------------------------------------------------------------
 * This lab uses SPI in polled mode. The hardware diagram has a loop back connection hard coded where SPI1 - MASTER and SPI0 - SLAVE.
 * In this code SPI1 MASTER writes to SPI0 slave. The data received by SPI0 is transmitted back to the SPI1 master.
 * The driver function used to achieve this are provided by the Xilinx as xspips.h and xspipshw_h.
 * They are present in the provided initialization.h header file.
 *
 * There are two commands in the menu (options in the menu).
 * 1. Enable loop back for UART manager task
 * 2. Enable loop back for spi1-spi0 connection enable or disable(loop back mode)
 *
 * User enters the command in following ways:
 * For example, after you load the application on to the board, User may wish to execute the menu command 1. command is only detected by using "enter, command, enter". In this it is <ENTER> <1> <ENTER>.
 *
 * Menu command 1:
 * Initially,
 * <ENTER><1><ENTER> can be used to change the UART Manager task loop back from enable to disable mode. This can also be done using the <ENTER><%><ENTER>.
 * To change the UART Manager loop back from disable to enable mode, use <ENTER><1><ENTER>.
 *
 * Menu command 2:
 * Initially,
 * <ENTER><2><ENTER> enables the task two loop back. Which means there is no connection between SPI 1-SPI 0.
 * You can enable the SPI 1 - SPI 0 connection using <ENTER><2><ENTER> or <ENTER><%><ENTER>.
 * However, to switch from SPI mode to loop back mode for menu command 2, you must use <ENTER><2><ENTER>.
 *
 * -----------------------------------------------
 *	When "task1_uart_loopback_en=0", UART Manager Task loop back is disabled. So, nothing will appear as an output on the console.
 *	When "task1_uart_loopback_en=1", UART Manager Task loop back is enabled. This will send the received characters on the console using the TaskUartManager() itself.
 *
 *  When "spi_master_loopback_en=1", there is no SPI connection in effect. The data will be echoed back to the user using the loop back mode that uses FIFO2.
 *  When "spi_master_loopback_en=0", SPI connection is in effect. The data entered by the user will loop back from spi1 to spi0 and again spi1. The data will be then displayed on the console using the FIFO2.
 * -----------------------------------------------
 *
 *	INITIALLY, when you run the application, the console will display the MENU and if you type anything from the console, nothing will be displayed.
 *	Type say, <ENTER><1><ENTER> to enable the UART loopback inside the task1. Now type any text from the console and it will be echoed back.
 *
 */
/******************************************************************************/


/*****************************  FreeRTOS includes. ****************************/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
/***************************** Include Files *********************************/

#include "xparameters.h"	/* SDK generated parameters */
#include "xspips.h"			/* SPI device driver */
#include "xil_printf.h"
#include <ctype.h>

/********************** User defined Include Files **************************/
#include "initialization.h"

/************************** Constant Definitions *****************************/
#define CHAR_PERCENT			0x25	/* '%' character is used as termination sequence */
#define CHAR_CARRIAGE_RETURN	0x0D	/* '\r' character is used in the termination sequence */
#define DOLLAR 					0x24	// BELL character

#define UART_DEVICE_ID_0		XPAR_XUARTPS_0_DEVICE_ID
#define SPI_0_DEVICE_ID			XPAR_XSPIPS_0_DEVICE_ID
#define SPI_1_DEVICE_ID			XPAR_XSPIPS_1_DEVICE_ID

/************************* Task Function definitions *************************/
static void TaskUartManager( void *pvParameters );
static TaskHandle_t xTask_uart;

static void TaskSpi1Controller( void *pvParameters );
static TaskHandle_t xTask_spi0;

static void TaskSpi0Peripheral( void *pvParameters );
static TaskHandle_t xTask_spi1;

/************************* Queue Function definitions *************************/

static QueueHandle_t xQueue_FIFO1 = NULL;//queue between task1 and task2
static QueueHandle_t xQueue_FIFO2 = NULL;//queue between task1 and task2

/************************* Function prototypes *************************/
void executeSpiMasterCommand(void);
void toggleUARTLoopback(void);
void toggleSpiMasterLoopback(void);
void executeTerminationSequence(void);
void checkForTerminationSequence(void);
void checkForValidCommand(void);

/************************* Global Variables *********************************/

BaseType_t spi_master_loopback_en = 0;	//GLOBAL variable to enable/disable loopback for spi_master() task (i.e., task number 2)
BaseType_t task1_uart_loopback_en=0;

u8 OPTIONS_IN_MENU = '2';				//total options in the menu display
u32 TRANSFER_SIZE_IN_BYTES = 1;			//1 bytes transferred between SPI 1 and SPI 0 in the provided template every time
u32 flag=0; 							//to enables sending dummy char in SPI1-SPI0 mode
u32 current_command_execution_flag=0;
u32 end_seq;

int str_length; 						//length of number of bytes string and the cpu statistics table

u8 RecvChar=0;
u8 RecvChar_1=0;
u8 task1_receive_from_FIFO2;
u8 task1_receive_from_FIFO2_spi_data;
u32 end_sequence_detect_flag=0;
u32 valid_command_detect_flag=0;
int message_counter=0;


int main(void)
{
	int Status;

	xTaskCreate( 	TaskUartManager,
					( const char * ) "TASK1",
					configMINIMAL_STACK_SIZE*10,
					NULL,
					tskIDLE_PRIORITY+4,
					&xTask_uart );

	xTaskCreate( 	TaskSpi1Controller,
					( const char * ) "TASK2",
					configMINIMAL_STACK_SIZE*10,
					NULL,
					tskIDLE_PRIORITY+3,
					&xTask_spi0 );

	xTaskCreate( 	TaskSpi0Peripheral,
					( const char * ) "TASK3",
					configMINIMAL_STACK_SIZE*10,
					NULL,
					tskIDLE_PRIORITY+3,
					&xTask_spi1 );


	xQueue_FIFO1 = xQueueCreate( 500,sizeof( u8 ) ); //connects task1 -> task2
	xQueue_FIFO2 = xQueueCreate( 500,sizeof( u8 ) ); //connects task2 -> task1


	/* Check the xQueue_FIFO1 and xQueue_FIFO2 if they were created. */
	configASSERT(xQueue_FIFO1);
	configASSERT(xQueue_FIFO2);

	//Initialization function for UART
	Status = intializeUART(UART_DEVICE_ID_0);
	if (Status != XST_SUCCESS) {
		xil_printf("UART Initialization Failed\r\n");
	}

	vTaskStartScheduler();

	while(1);

	return 0;
}


static void TaskUartManager( void *pvParameters ){

	print_command_menu();

	while(1){

		/* Wait until there is data */
		if(flag==1){ 						// flag is set to 1 when the user enters the end sequence on SPI0-SPI1 mode
			u8 dummy = DOLLAR; 				// dummy char to send to the slave as a control character
			int loop_counter = str_length; 	// loop count for sending the dummy char
			for(int i=0;i<loop_counter;i++){

				/*******************************************/
				//write the logic to send the "dummy" control character using the FIFO1
				//Also wait on to receive the bytes coming from the SPIMaster task
				//If there is space on the Transmitter UART side, send it to the UART using an appropriate UART write function.

				xQueueSendToBack(xQueue_FIFO1,&dummy,0UL);
				xQueueReceive(xQueue_FIFO2, &task1_receive_from_FIFO2, portMAX_DELAY);
				while (XUartPs_IsTransmitFull(XPAR_XUARTPS_0_BASEADDR) == TRUE){};
				XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, task1_receive_from_FIFO2);
				/*******************************************/
			}
			flag=0;
		}
		else{
			while (XUartPs_IsReceiveData(XPAR_XUARTPS_0_BASEADDR)){
				RecvChar_1 = RecvChar;
				RecvChar = XUartPs_ReadReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET);

				if(current_command_execution_flag == 2){
					// This function is called when command 2 is selected by the user. Read the function definition for more information...
					executeSpiMasterCommand();
				}
				// check if the user input is a valid command (excluding termination sequence), either \r1\r or \r2\r
				// if so, increment valid_command_detect_flag to 3
				checkForValidCommand();

				// only if a valid command is detected
				if(valid_command_detect_flag == 3){
					if(RecvChar_1 == '1'){
						toggleUARTLoopback();
					}
					else if(RecvChar_1 == '2'){
						toggleSpiMasterLoopback();
					}
				}

				if((current_command_execution_flag == 1 && task1_uart_loopback_en == 1) | (current_command_execution_flag == 2 && spi_master_loopback_en==1)){
					// check if the user entered the termination sequence
					// if so, increment end_sequence_detect_flag to 3
					checkForTerminationSequence();

					if(current_command_execution_flag == 1 && task1_uart_loopback_en == 1){

						XUartPs_SendByte(XPAR_XUARTPS_0_BASEADDR, RecvChar);
					}
				}

				if(end_sequence_detect_flag == 3){
					// Once termination sequence is detected, disable the loopback mode for either task 1 or 2 depending on the current menu command selected by the user
					executeTerminationSequence();
				}
			}
		}

	vTaskDelay(1);
	}
}
static void TaskSpi1Controller( void *pvParameters ){

	u8 task2_receive_from_FIFO1;
	u8 send_SPI_data_via_FIFO2;
	u32 bytecount=0;
	u8 send_buffer[1];

	Initialize_SPI_0_and_1(SPI_0_DEVICE_ID,SPI_1_DEVICE_ID);

	while(1){
		xQueueReceive( 	xQueue_FIFO1,
						&task2_receive_from_FIFO1, 										//queue to receive the data from UART Manager task
						portMAX_DELAY );

		if(spi_master_loopback_en==1 && current_command_execution_flag==2) 				//just send the characters back to task 1. No SPI access! This is because loopback is currently enabled.
			xQueueSendToBack(xQueue_FIFO2,&task2_receive_from_FIFO1,0UL);
		else if ((spi_master_loopback_en==0) && (current_command_execution_flag==2)){ 	//if global variable spi_master_loopback_en=0, we enable SPI connection mode now!
			/*******************************************/
			//write the code here to copy the received data from the FIFO1 into "sendbuffer" variable. The "sendbuffer" variable is declared for you.
			//You want to transfer the bytes based on the TRANSFER_SIZE_IN_BYTES value.
			//You can use the write function for MasterSPI (from the driver file) for that and then you want to use task_YIELD() that allows the slave SPI task to work.
			//Finally, you want to use the read from master implementation using the function from the driver file provided.
			//Then send the data to the back of the FIFO2 and reset the "bytecount" variable to zero.

			send_buffer[bytecount] = task2_receive_from_FIFO1;
			bytecount++;
			if(bytecount == TRANSFER_SIZE_IN_BYTES){									//byte is read from the FIFO1 and then it is transferred to the SPI connection
				SpiMasterWrite(send_buffer, TRANSFER_SIZE_IN_BYTES);					//the byte read back from the SPI master Rx FIFO is then given back to the UART Manager task using FIFO2
				taskYIELD();
				send_SPI_data_via_FIFO2 = SpiMasterRead(TRANSFER_SIZE_IN_BYTES);
				xQueueSendToBack(xQueue_FIFO2,&send_SPI_data_via_FIFO2,0UL);
				bytecount=0;
			}

			/*******************************************/

		}
		vTaskDelay(1);
	}
}

static void TaskSpi0Peripheral( void *pvParameters ){
	u32 end_sequence_flag=0;
	u8 temp_store;
	int num_received = 0;
	char buffer[150];

	while(1){

		/*******************************************/
		//Write the following logic here:
		//Detect the termination sequence. We also keep track of number of characters received via SPI connection along with the messages received so far.
		//See that we enclose entire logic in this task inside the given "if" condition based on the "spi_master_loopback_en" and  "current_command_execution_flag".
		//You want to also use SpiSlave read and write functions from the driver file in this task.
		//"RxBuffer_Slave" variable is used to read the slave. Have a look once at this variable in the driver file.
		//"end_sequence_flag" variable is set to 3 when termination sequence is successfully detected.
		//"buffer" array is used to store the message string ::: "The number of characters received over SPI:%d\nNumber of messages received so far across SPI: %d\n"
		//Make sure that until the termination sequence, you want to keep on sending the bytes back to SPI master.
		//Once \r%\r is detected you want to now send the message string and you may use a looping method to send it to the SPI master.

		//this is the logic to detect the termination sequence. We also keep track of number of characters received via SPI connection.
		if(spi_master_loopback_en==0 && current_command_execution_flag==2){
			SpiSlaveRead(TRANSFER_SIZE_IN_BYTES);
			num_received ++;
			// detecting the end sequence
			if(*RxBuffer_Slave == CHAR_CARRIAGE_RETURN && end_sequence_flag==2){
				end_sequence_flag +=1;
			}
			else if(*RxBuffer_Slave == CHAR_PERCENT && end_sequence_flag==1){
				end_sequence_flag +=1;
			}
			else if(*RxBuffer_Slave == CHAR_CARRIAGE_RETURN && end_sequence_flag==0){
				end_sequence_flag +=1;
			}
			else{
//				num_received ++; // counting the numbers of non-control characters
				end_sequence_flag=0;
			}

			/////////////////////////
			//If termination sequence is received, i.e., end_sequence_flag=3, we will create the message string "The number of characters received over SPI:%d\nNumber of messages received so far across SPI: %d\n" and send it back to using the SpiSlave() func.
			//This will further be passed on the SPI master task -> UART task using the FIFO2, and then it will also be displayed on the terminal console.

			if (end_sequence_flag == 3){

				flag = 1;
				message_counter++;
				sprintf(buffer,"The number of characters received over SPI:%d\nNumber of messages received so far across SPI: %d\n", num_received-2, message_counter);
				str_length = (int) strlen(buffer);

				for (int i = 0; i< str_length; i++){
					SpiSlaveRead(TRANSFER_SIZE_IN_BYTES);			// read a dummy char from the master
					temp_store = buffer[i];
					SpiSlaveWrite(&temp_store, TRANSFER_SIZE_IN_BYTES);	// write a char from number of bytes string to the master
				}

				SpiSlaveRead(TRANSFER_SIZE_IN_BYTES);
				end_sequence_flag = 0;
				num_received = 0;
				flag = 0;

			}
			//////////////////////////////////////////
			buffer_Rx_Master = RxBuffer_Slave;
			SpiSlaveWrite(buffer_Rx_Master, TRANSFER_SIZE_IN_BYTES);
		}

		/*******************************************/

		vTaskDelay(1);
	}
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
///////////////////////////////////////////////////////////////////////////

/**
 * This function is called when command 2 is selected by the user.
 * If the loopback is enabled, the data sent travels between task1 and task2 only and is received back and written to the UART using task1.
 * If the loopback is disabled, the SPI connection is in effect and the data is communicate from taskUART -> taskSPImaster -> taskSPIslave and again it travels all the way back to UARTManager task.
 * "task1_receive_from_FIFO2" variable is used when loopback is enabled, to read the FIFO2 data.
 * "task1_receive_from_FIFO2_spi_data" variable is used when SPI connection is enabled to read the data from the FIFO2.
 * Returns: None
 */
void executeSpiMasterCommand(void){

	xQueueSendToBack(xQueue_FIFO1,&RecvChar,0UL);

	if(spi_master_loopback_en == 1){
		xQueueReceive(xQueue_FIFO2, &task1_receive_from_FIFO2, portMAX_DELAY);
		while (XUartPs_IsTransmitFull(XPAR_XUARTPS_0_BASEADDR) == TRUE){};
		XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, task1_receive_from_FIFO2);
	}

	/*******************************************/
	//write the "else" condition here for the case when "spi_master_loopback_en=0"
	//you want to receive the data from FIFO2 into the variable "task1_receive_from_FIFO2_spi_data"
	//If there is space in the UART transmitter, write the byte to the UART
	else{
		xQueueReceive(xQueue_FIFO2, &task1_receive_from_FIFO2_spi_data, portMAX_DELAY);
		while (XUartPs_IsTransmitFull(XPAR_XUARTPS_0_BASEADDR) == TRUE){};
		XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, task1_receive_from_FIFO2_spi_data);
	}

	/*******************************************/

}

/**
 * This functions toggles the loopback mode for task 1 when command 1 is selected by the user
 * Returns: None
 */
void toggleUARTLoopback(void){

	current_command_execution_flag = 1;
	task1_uart_loopback_en = !task1_uart_loopback_en;
	if(task1_uart_loopback_en==1){
		xil_printf("\n*** UART Manager Task loopback enabled ***\r\n");
	}
	else
		xil_printf("\n*** UART Manager Task loopback disabled using command toggling***\r\n");
}

/**
 * This functions toggles the loopback mode for task 2 when command 2 is selected by the user
 * Returns: None
 */
void toggleSpiMasterLoopback(void){

	current_command_execution_flag = 2;
	spi_master_loopback_en = !spi_master_loopback_en;
	if(spi_master_loopback_en==1){
		xil_printf("\n*** Task2 loopback enabled : No access to SPI0 interface ***\r\n");
	}
	else{
		xil_printf("\n*** Task2 loopback disabled using command toggling : SPI0-SPI1 in effect. Send the bytes from the console ***\r\n");
	}
}

/**
 * This function is called when the termination sequence is detected.
 * It disables the loopback mode of the UART Manager or Spi Master, depending on which command was selected at the time of termination
 * Returns: None
 */
void executeTerminationSequence(void){

	end_sequence_detect_flag = 0;
	if(current_command_execution_flag == 1){
		task1_uart_loopback_en = 0;
		xil_printf("\n*** UART Manager Task loopback disabled using termination sequence***\r\n");
	}
	else if(current_command_execution_flag == 2){
		spi_master_loopback_en = 0;
		xil_printf("\n*** Task2 loopback disabled using termination sequence : SPI1-SPI0 connection in effect. Send the bytes from the console ***\r\n");
	}
}

/**
 * This functions checks if the user input is the termination sequence \r%\r
 * When the termination sequence is detected, end_sequence_detect_flag would be 3
 * Returns: None
 */
void checkForTerminationSequence(void){

	if(RecvChar == CHAR_CARRIAGE_RETURN && end_sequence_detect_flag==2){
		end_sequence_detect_flag +=1;
	}
	else if(RecvChar == CHAR_PERCENT && end_sequence_detect_flag==1)
		end_sequence_detect_flag +=1;
	else if(RecvChar == CHAR_CARRIAGE_RETURN && end_sequence_detect_flag==0)
		end_sequence_detect_flag +=1;
	else
		end_sequence_detect_flag=0;
}

/**
 * This functions checks if the user input is a valid command or not. A valid command would be ENTER COMMAND# ENTER.
 * COMMAND# can only be 1 or 2
 * For a valid command, the valid_command_detect_flag would be 3
 * Returns: None
 */
void checkForValidCommand(void){

	if((RecvChar == 0x0D) && valid_command_detect_flag==0){
		valid_command_detect_flag = valid_command_detect_flag + 1;
	}
	else if((isdigit((int)RecvChar) && ((int)RecvChar <= OPTIONS_IN_MENU)) && valid_command_detect_flag == 1){
		valid_command_detect_flag = valid_command_detect_flag + 1;
	}
	else if(RecvChar == 0x0D){
		valid_command_detect_flag = valid_command_detect_flag + 1;
	}
	else
		valid_command_detect_flag = 0;

}


