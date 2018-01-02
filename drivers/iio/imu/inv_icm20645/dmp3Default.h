/*
*
* Copyright @ 2011-2014 InvenSense Inc.  All rights reserved.
*
* This software and/or documentation  (collectively ��Software��) is subject to
* InvenSense intellectual property rights
* under U.S. and international copyright and other intellectual property rights laws.
*
* The Software contained herein is PROPRIETARY and CONFIDENTIAL to InvenSense and is provided
* solely under the terms and conditions of a form of InvenSense software license agreement between
* InvenSense and you and any use, modification, reproduction or disclosure of the Software without
* such agreement or the express written consent of InvenSense is strictly prohibited.
*
* EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE SOFTWARE IS
* PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
* EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
* INVENSENSE BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
* OF THE SOFTWARE.
*
*/

#define SC_AUT_INIT				(17 * 16 + 4)

#define SC_AUT_INPUT_20645			(132 * 16 + 0)
#define SC_AUT_OUTPUT_20645			(132 * 16 + 4)

#define SC_AUT_INPUT_10340			(155 * 16 + 8)
#define SC_AUT_OUTPUT_10340			(155 * 16 + 12)

#define DATA_OUT_CTL1			(4 * 16)
#define DATA_OUT_CTL2			(4 * 16 + 2)
#define DATA_INTR_CTL			(4 * 16 + 12)

#define MOTION_EVENT_CTL		(4 * 16 + 14)

#define BM_BATCH_CNTR			(27 * 16)
#define BM_BATCH_THLD			(19 * 16 + 12)
#define BM_BATCH_MASK			(21 * 16 + 14)
#define BM_FIFO_SIZE			(17 * 16 + 6)

#define ODR_ACCEL				(11 * 16 + 14)
#define ODR_GYRO				(11 * 16 + 10)
#define ODR_CPASS				(11 * 16 +  6)
#define ODR_ALS					(11 * 16 +  2)
#define ODR_QUAT6				(10 * 16 + 12)
#define ODR_PQUAT6				(10 * 16 +  4)
#define ODR_PRESSURE			(11 * 16 + 12)
#define ODR_GYRO_CALIBR			(11 * 16 +  8)

#define ODR_CNTR_ACCEL			(9 * 16 + 14)
#define ODR_CNTR_GYRO			(9 * 16 + 10)
#define ODR_CNTR_CPASS			(9 * 16 +  6)
#define ODR_CNTR_ALS			(9 * 16 +  2)
#define ODR_CNTR_QUAT6			(8 * 16 + 12)
#define ODR_CNTR_PQUAT6			(8 * 16 +  4)
#define ODR_CNTR_PRESSURE		(9 * 16 + 12)
#define ODR_CNTR_GYRO_CALIBR	(9 * 16 +  8)

#define CPASS_MTX_00            (23 * 16)
#define CPASS_MTX_01            (23 * 16 + 4)
#define CPASS_MTX_02            (23 * 16 + 8)
#define CPASS_MTX_10            (23 * 16 + 12)
#define CPASS_MTX_11            (24 * 16)
#define CPASS_MTX_12            (24 * 16 + 4)
#define CPASS_MTX_20            (24 * 16 + 8)
#define CPASS_MTX_21            (24 * 16 + 12)
#define CPASS_MTX_22            (25 * 16)

#define GYRO_SF					(19 * 16)
#define ACCEL_FB_GAIN			(34 * 16)
#define ACCEL_ONLY_GAIN			(16 * 16 + 12)

#define GYRO_BIAS_X				(139 * 16 +  4)
#define GYRO_BIAS_Y				(139 * 16 +  8)
#define GYRO_BIAS_Z				(139 * 16 + 12)
#define GYRO_ACCURACY			(138 * 16 +  2)
#define GYRO_BIAS_SET			(138 * 16 +  6)
#define GYRO_LAST_TEMPR			(134 * 16)
#define GYRO_SLOPE_X			(78 * 16 +  4)
#define GYRO_SLOPE_Y			(78 * 16 +  8)
#define GYRO_SLOPE_Z			(78 * 16 + 12)

#define ACCEL_BIAS_X            (161 * 16 +  4)
#define ACCEL_BIAS_Y            (161 * 16 +  8)
#define ACCEL_BIAS_Z            (161 * 16 + 12)
#define ACCEL_ACCURACY			(151 * 16)
#define ACCEL_CAL_RESET			(160 * 16)
#define ACCEL_VARIANCE_THRESH	(164 * 16)
#define ACCEL_CAL_RATE			(149 * 16 + 4)
#define ACCEL_PRE_SENSOR_DATA	(151 * 16 + 4)
#define ACCEL_COVARIANCE		(152 * 16)
#define ACCEL_ALPHA_VAR			(147 * 16)
#define ACCEL_A_VAR				(148 * 16)
#define ACCEL_CAL_INIT			(149 * 16 + 2)

#define CPASS_BIAS_X            (126 * 16 +  4)
#define CPASS_BIAS_Y            (126 * 16 +  8)
#define CPASS_BIAS_Z            (126 * 16 + 12)

#define CPASS_STATUS_CHK		(25 * 16 + 12)

#define DMPRATE_CNTR			(18 * 16 + 4)

#define PEDSTD_BP_B				(49 * 16 + 12)
#define PEDSTD_BP_A4			(52 * 16)
#define PEDSTD_BP_A3			(52 * 16 +  4)
#define PEDSTD_BP_A2			(52 * 16 +  8)
#define PEDSTD_BP_A1			(52 * 16 + 12)
#define PEDSTD_SB				(50 * 16 +  8)
#define PEDSTD_SB_TIME			(50 * 16 + 12)
#define PEDSTD_PEAKTHRSH		(57 * 16 +  8)
#define PEDSTD_TIML				(50 * 16 + 10)
#define PEDSTD_TIMH				(50 * 16 + 14)
#define PEDSTD_PEAK				(57 * 16 +  4)
#define PEDSTD_STEPCTR			(54 * 16)
#define PEDSTD_STEPCTR2			(58 * 16 +  8)
#define PEDSTD_TIMECTR			(60 * 16 +  4)
#define PEDSTD_DECI				(58 * 16)
#define PEDSTD_SB2				(60 * 16 + 14)
#define STPDET_TIMESTAMP		(18 * 16 +  8)
#define PEDSTD_DRIVE_STATE		(43 * 16 + 10)
#define PED_RATE				(58 * 16 +  4)

#define WOM_ENABLE              (140 * 16 + 14)
#define WOM_STATUS              (140 * 16 + 6)
#define WOM_THRESHOLD           (140 * 16)
#define WOM_CNTR_TH             (140 * 16 + 12)

#define BAC_RATE                (48  * 16 +  8)
#define TILT_ONLY               (68  * 16 + 12)
#define BAC_STATE               (115 * 16 +  0)
#define BAC_STATE_PREV          (115 * 16 +  4)
#define BAC_ACT_ON              (118 * 16 +  0)
#define BAC_ACT_OFF             (119 * 16 +  0)
#define BAC_STILL_S_F           (113 * 16 +  0)
#define BAC_RUN_S_F             (113 * 16 +  4)
#define BAC_DRIVE_S_F           (114 * 16 +  0)
#define BAC_WALK_S_F            (114 * 16 +  4)
#define BAC_SMD_S_F             (114 * 16 +  8)
#define BAC_BIKE_S_F            (114 * 16 + 12)
#define BAC_E1_SHORT            (82  * 16 +  0)
#define BAC_E2_SHORT            (82  * 16 +  4)
#define BAC_E3_SHORT            (82  * 16 +  8)
#define BAC_E4_SHORT            (82  * 16 + 12)
#define BAC_VAR_RUN             (84  * 16 + 12)
#define BAC_TILT_INIT           (117 * 16 +  0)
#define BAC_SMD_ST_TH           (115 * 16 +  8)
#define BAC_ST_ALPHA4           (116 * 16 + 12)
#define BAC_ST_ALPHA4A          (112 * 16 + 12)
#define SMD_VAR_TH              (75  * 16 + 12)
#define SMD_VAR_TH_DRIVE        (76  * 16 + 12)
#define SMD_DRIVE_TIMER_TH      (76  * 16 +  8)
#define SMD_TILT_ANGLE_TH       (115 * 16 + 12)
#define BAC_GYRO_ON             (105 * 16 +  4)
#define BAC_BIKE_PREFERENCE     (109 * 16 +  8)

#define DROPDET_MIN_FALL_TIME	(81 * 16 +  4)
#define DROPDET_IMPACT_THRESH	(81 * 16 +  8)
#define DROPDET_FALL_THRESH		(81 * 16 + 12)

#define FP_VAR_ALPHA            (70 * 16 +  8)
#define FP_STILL_TH             (71 * 16 +  4)
#define FP_MID_STILL_TH         (69 * 16 +  8)
#define FP_NOT_STILL_TH         (71 * 16 +  8)
#define FP_VIB_REJ_TH           (65 * 16 +  8)
#define FP_MAX_PICKUP_T_TH      (69 * 16 + 12)
#define FP_PICKUP_TIMEOUT_TH    (79 * 16 +  8)
#define FP_STILL_CONST_TH       (71 * 16 + 12)
#define FP_MOTION_CONST_TH      (64 * 16 +  8)
#define FP_VIB_COUNT_TH         (66 * 16 +  8)
#define FP_STEADY_TILT_TH       (75 * 16 +  8)
#define FP_STEADY_TILT_UP_TH    (66 * 16 + 12)
#define FP_Z_FLAT_TH_MINUS      (68 * 16 +  8)
#define FP_Z_FLAT_TH_PLUS       (68 * 16 +  4)
#define FP_DEV_IN_POCKET_TH     (46 * 16 + 12)
#define FP_PICKUP_CNTR          (74 * 16 +  4)
#define FP_FLIP_CNTR            (75 * 16 +  4)
#define FP_RATE                 (64 * 16 + 12)

#define ACC_SCALE               (16 * 16 +  8)

#define S_HEALTH_WALK_RUN_1		(16*137 + 12)
#define S_HEALTH_WALK_RUN_2		(16*137 +  8)
#define S_HEALTH_WALK_RUN_3		(16*137 +  4)
#define S_HEALTH_WALK_RUN_4		(16*137 +  0)
#define S_HEALTH_WALK_RUN_5		(16*136 + 12)
#define S_HEALTH_WALK_RUN_6		(16*136 +  8)
#define S_HEALTH_WALK_RUN_7		(16*136 +  4)
#define S_HEALTH_WALK_RUN_8		(16*136 +  0)
#define S_HEALTH_WALK_RUN_9		(16*135 + 12)
#define S_HEALTH_WALK_RUN_10	(16*135 +  8)
#define S_HEALTH_WALK_RUN_11	(16*135 +  4)
#define S_HEALTH_WALK_RUN_12	(16*135 +  0)
#define S_HEALTH_WALK_RUN_13	(16*133 + 12)
#define S_HEALTH_WALK_RUN_14	(16*133 +  8)
#define S_HEALTH_WALK_RUN_15	(16*133 +  4)
#define S_HEALTH_WALK_RUN_16	(16*133 +  0)
#define S_HEALTH_WALK_RUN_17	(16*130 + 12)
#define S_HEALTH_WALK_RUN_18	(16*130 +  8)
#define S_HEALTH_WALK_RUN_19	(16*130 +  4)
#define S_HEALTH_WALK_RUN_20	(16*130 +  0)
#define S_HEALTH_CADENCE1		(16*137 + 14)
#define S_HEALTH_CADENCE2		(16*137 + 10)
#define S_HEALTH_CADENCE3		(16*137 +  6)
#define S_HEALTH_CADENCE4		(16*137 +  2)
#define S_HEALTH_CADENCE5		(16*136 + 14)
#define S_HEALTH_CADENCE6		(16*136 + 10)
#define S_HEALTH_CADENCE7		(16*136 +  6)
#define S_HEALTH_CADENCE8		(16*136 +  2)
#define S_HEALTH_CADENCE9		(16*135 + 14)
#define S_HEALTH_CADENCE10		(16*135 + 10)
#define S_HEALTH_CADENCE11		(16*135 +  6)
#define S_HEALTH_CADENCE12		(16*135 +  2)
#define S_HEALTH_CADENCE13		(16*133 + 14)
#define S_HEALTH_CADENCE14		(16*133 + 10)
#define S_HEALTH_CADENCE15		(16*133 +  6)
#define S_HEALTH_CADENCE16		(16*133 +  2)
#define S_HEALTH_CADENCE17		(16*130 + 14)
#define S_HEALTH_CADENCE18		(16*130 + 10)
#define S_HEALTH_CADENCE19		(16*130 +  6)
#define S_HEALTH_CADENCE20		(16*130 +  2)
#define S_HEALTH_INT_PERIOD		(16*142 +  6)
#define S_HEALTH_INT_PERIOD2	(16*142 + 10)
#define S_HEALTH_BACKUP1		(16*142 +  0)
#define S_HEALTH_BACKUP2		(16*142 +  2)
#define S_HEALTH_RATE           (16*128 + 14)

#define CPASS_TIMER_TH_LO       (16*16 + 4)
#define CPASS_TIMER_TH_HI       (16*17 + 4)
#define CPASS_I2C_ADDR          (16*17 + 2)


#define ACCEL_MASK		0x80
#define GYRO_MASK		0x40
#define CPASS_MASK		0x20
#define ALS_MASK		0x10
#define QUAT6_MASK		0x08
#define QUAT9_MASK		0x04
#define PQUAT6_MASK		0x02
#define FOOTER_MASK		0x01
#define PRESSURE_MASK	0x80
#define GYRO_CALIBR_MASK	0x40
#define CPASS_CALIBR_MASK	0x20
#define PED_STEPDET_MASK	0x10
#define HEADER2_MASK		0x08
#define PED_STEPIND_MASK	0x07

#define ACCEL_SET		0x8000
#define GYRO_SET		0x4000
#define CPASS_SET		0x2000
#define ALS_SET			0x1000
#define QUAT6_SET		0x0800
#define PQUAT6_SET		0x0200
#define FOOTER_SET		0x0100
#define PRESSURE_SET	0x0080
#define GYRO_CALIBR_SET	0x0040
#define PED_STEPDET_SET	0x0010
#define HEADER2_SET		0x0008
#define PED_STEPIND_SET 0x0007

#define ACCEL_ACCURACY_MASK	0x40
#define GYRO_ACCURACY_MASK	0x20
#define COMPASS_CAL_INPUT_MASK	0x10
#define DROP_DETECTION_MASK	0X08
#define FLIP_PICKUP_MASK    0x04
#define GEOMAG_MASK			0x02
#define BATCH_MODE_MASK		0x01
#define ACT_RECOG_MASK		0x80
#define GYRO_OFF_MASK		0x40

#define ACCEL_ACCURACY_SET	0x4000
#define GYRO_ACCURACY_SET	0x2000
#define COMPASS_CAL_INPUT_SET	0x1000
#define DROP_DETECTION_SET	0X0800
#define FLIP_PICKUP_SET     0x0400
#define GEOMAG_EN			0x0200
#define BATCH_MODE_EN		0x0100
#define ACT_RECOG_SET       0x0080
#define GYRO_AUTO_OFF_SET   0x0040

/* high byte of motion event control */
#define PEDOMETER_EN		0x40
#define PEDOMETER_INT_EN	0x20
#define TILT_INT_EN			0x10
#define SMD_EN				0x08
#define FLIP_PICKUP_EN		0x04
#define ACCEL_CAL_EN		0x02
#define GYRO_CAL_EN			0x01
/* low byte of motion event control */
#define COMPASS_CAL_EN		0x80
#define S_HEALTH_EN        0x40
#define GYRO_AUTO_OFF_EN   0x20

#define HEADER_SZ		2
#define ACCEL_DATA_SZ	12
#define GYRO_DATA_SZ	6
#define CPASS_DATA_SZ	10
#define ALS_DATA_SZ		8
#define QUAT6_DATA_SZ	12
#define QUAT9_DATA_SZ	14
#define PQUAT6_DATA_SZ	6
#define PRESSURE_DATA_SZ		6
#define GYRO_CALIBR_DATA_SZ		12
#define CPASS_CALIBR_DATA_SZ	12
#define PED_STEPDET_TIMESTAMP_SZ	4
#define FOOTER_SZ		2

#define HEADER2_SZ			2
#define ACCEL_ACCURACY_SZ	2
#define GYRO_ACCURACY_SZ	2
#define DROP_DETECTION_SZ	2
#define FLIP_PICKUP_SZ      2
#define ACT_RECOG_SZ        6
#define GYRO_AUTO_OFF_SZ    2
