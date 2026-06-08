/*
 * w35t51nw.h
 *
 *  Created on: Jun 6, 2026
 *      Author: Huang Jian
 */

#ifndef W35T51NW_H_
#define W35T51NW_H_

#define W35T51NW_SELF_TEST      (0) /* Full bring-up self-test (erase/write/read + mmap) */
#define W35T51NW_DDR_TUNE       (0) /* Scan DDR dummy/DQS window and pick a working point */
#define W35T51NW_DDR_ENABLE     (1) /* Switch production read path to DDR OPI */

void W35T51NW_LowLevelProbe(void);
void W35T51NW_EnableDdrXipMode(void);
#if W35T51NW_SELF_TEST
void W35T51NW_ExtFlashSelfTest(void);
#endif /* W35T51NW_SELF_TEST */

#endif /* W35T51NW_H_ */
