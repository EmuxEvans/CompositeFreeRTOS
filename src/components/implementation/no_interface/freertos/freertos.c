#include <string.h>
#include <cos_component.h>
#include <bitmap.h>

#define FREERTOS_MAX_MEMORY 900

extern int freeRTOS_entry( void );

void print(char *str) {
	cos_print(str, strlen(str));
}

void cos_init(void)
{
	vaddr_t *memory, test;
	int i;
	test = "Hello, World from component!\n";
	print(test);
	
	freeRTOS_entry();
	
	/* memory = NULL; */
	/* for (i = 0; i < FREERTOS_MAX_MEMORY; i++) { */
	/* 	print("Allocating page... \n"); */
	/* 	vaddr_t page = cos_get_vas_page(); */
	/* 	if (!memory) memory = page; */

	/* 	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), page, i)) { */
	/* 		print("Error allocating memory\n"); */
	/* 		return; */
	/* 	} */
	/* } */
	return;
}
