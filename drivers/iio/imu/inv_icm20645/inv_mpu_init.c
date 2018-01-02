/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "inv_mpu_iio.h"

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20645"},
	{128, "ICM10340"},
};

static int inv_set_dmp(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH,
						st->dmp_start_address >> 8);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH + 1,
						st->dmp_start_address & 0xff);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_calc_gyro_sf(s8 pll)
{
        int a, r;
        int value, t;

        t = 102870L + 81L * pll;
        a = (1L << 30) / t;
        r = (1L << 30) - a * t;
        value = a * 797 * DMP_DIVIDER;
        value += (s64)((a * 1011387LL * DMP_DIVIDER) >> 20);
        value += r * 797L * DMP_DIVIDER / t;
        value += (s32)((s64)((r * 1011387LL * DMP_DIVIDER) >> 20)) / t;
        value <<= 1;

        return value;
}

static int inv_read_timebase(struct inv_mpu_state *st)
{
	int result;
	u8 d;
	s8 t;

	result = inv_set_bank(st, BANK_SEL_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_TIMEBASE_CORRECTION_PLL, 1, &d);
	if (result)
		return result;
	t = abs(d & 0x7f);
	if (d & 0x80)
		t = -t;

	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC - t * 769903 +
						((t * 769903) / 1270) * t;
	st->eng_info[ENGINE_PRESSURE].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time  = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_PRESSURE].orig_rate = MAX_PRS_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	st->gyro_sf = inv_calc_gyro_sf(t);
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG_1,
				(st->chip_config.fsr << SHIFT_GYRO_FS_SEL) | 1);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
			(st->chip_config.accel_fs << SHIFT_ACCEL_FS) | 0);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_secondary(struct inv_mpu_state *st)
{
	int r;

	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_I2C_MST_CTRL, BIT_I2C_MST_P_NSR);
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
							MIN_MST_ODR_CONFIG);
	if (r)
		return r;

	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}
static int inv_init_secondary(struct inv_mpu_state *st)
{
	st->slv_reg[0].addr = REG_I2C_SLV0_ADDR;
	st->slv_reg[0].reg  = REG_I2C_SLV0_REG;
	st->slv_reg[0].ctrl = REG_I2C_SLV0_CTRL;
	st->slv_reg[0].d0   = REG_I2C_SLV0_DO;

	st->slv_reg[1].addr = REG_I2C_SLV1_ADDR;
	st->slv_reg[1].reg  = REG_I2C_SLV1_REG;
	st->slv_reg[1].ctrl = REG_I2C_SLV1_CTRL;
	st->slv_reg[1].d0   = REG_I2C_SLV1_DO;

	st->slv_reg[2].addr = REG_I2C_SLV2_ADDR;
	st->slv_reg[2].reg  = REG_I2C_SLV2_REG;
	st->slv_reg[2].ctrl = REG_I2C_SLV2_CTRL;
	st->slv_reg[2].d0   = REG_I2C_SLV2_DO;

	st->slv_reg[3].addr = REG_I2C_SLV3_ADDR;
	st->slv_reg[3].reg  = REG_I2C_SLV3_REG;
	st->slv_reg[3].ctrl = REG_I2C_SLV3_CTRL;
	st->slv_reg[3].d0   = REG_I2C_SLV3_DO;

	return 0;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size         = ACCEL_DATA_SZ;
	st->sensor[SENSOR_GYRO].sample_size          = GYRO_DATA_SZ;
	st->sensor[SENSOR_COMPASS].sample_size       = CPASS_DATA_SZ;
	st->sensor[SENSOR_ALS].sample_size           = ALS_DATA_SZ;
	st->sensor[SENSOR_PRESSURE].sample_size      = PRESSURE_DATA_SZ;
	st->sensor[SENSOR_SIXQ].sample_size          = QUAT6_DATA_SZ;
	st->sensor[SENSOR_PEDQ].sample_size          = PQUAT6_DATA_SZ;
	st->sensor[SENSOR_CALIB_GYRO].sample_size    = GYRO_CALIBR_DATA_SZ;

	st->sensor[SENSOR_ACCEL].odr_addr         = ODR_ACCEL;
	st->sensor[SENSOR_GYRO].odr_addr          = ODR_GYRO;
	st->sensor[SENSOR_COMPASS].odr_addr       = ODR_CPASS;
	st->sensor[SENSOR_ALS].odr_addr           = ODR_ALS;
	st->sensor[SENSOR_PRESSURE].odr_addr      = ODR_PRESSURE;
	st->sensor[SENSOR_SIXQ].odr_addr          = ODR_QUAT6;
	st->sensor[SENSOR_PEDQ].odr_addr          = ODR_PQUAT6;
	st->sensor[SENSOR_CALIB_GYRO].odr_addr    = ODR_GYRO_CALIBR;

	st->sensor[SENSOR_ACCEL].counter_addr         = ODR_CNTR_ACCEL;
	st->sensor[SENSOR_GYRO].counter_addr          = ODR_CNTR_GYRO;
	st->sensor[SENSOR_COMPASS].counter_addr       = ODR_CNTR_CPASS;
	st->sensor[SENSOR_ALS].counter_addr           = ODR_CNTR_ALS;
	st->sensor[SENSOR_PRESSURE].counter_addr      = ODR_CNTR_PRESSURE;
	st->sensor[SENSOR_SIXQ].counter_addr          = ODR_CNTR_QUAT6;
	st->sensor[SENSOR_PEDQ].counter_addr          = ODR_CNTR_PQUAT6;
	st->sensor[SENSOR_CALIB_GYRO].counter_addr    = ODR_CNTR_GYRO_CALIBR;

	st->sensor[SENSOR_ACCEL].output         = ACCEL_SET;
	st->sensor[SENSOR_GYRO].output          = GYRO_SET;
	st->sensor[SENSOR_COMPASS].output       = CPASS_SET;
	st->sensor[SENSOR_ALS].output           = ALS_SET;
	st->sensor[SENSOR_PRESSURE].output      = PRESSURE_SET;
	st->sensor[SENSOR_SIXQ].output          = QUAT6_SET;
	st->sensor[SENSOR_PEDQ].output          = PQUAT6_SET;
	st->sensor[SENSOR_CALIB_GYRO].output    = GYRO_CALIBR_SET;

	st->sensor[SENSOR_ACCEL].a_en           = true;
	st->sensor[SENSOR_GYRO].a_en            = false;
	st->sensor[SENSOR_COMPASS].a_en         = false;
	st->sensor[SENSOR_ALS].a_en             = false;
	st->sensor[SENSOR_PRESSURE].a_en        = false;
	st->sensor[SENSOR_SIXQ].a_en            = true;
	st->sensor[SENSOR_PEDQ].a_en            = true;
	st->sensor[SENSOR_CALIB_GYRO].a_en      = false;

	st->sensor[SENSOR_ACCEL].g_en         = false;
	st->sensor[SENSOR_GYRO].g_en          = true;
	st->sensor[SENSOR_COMPASS].g_en       = false;
	st->sensor[SENSOR_ALS].g_en           = false;
	st->sensor[SENSOR_PRESSURE].g_en      = false;
	st->sensor[SENSOR_SIXQ].g_en          = true;
	st->sensor[SENSOR_PEDQ].g_en          = true;
	st->sensor[SENSOR_CALIB_GYRO].g_en    = true;

	st->sensor[SENSOR_ACCEL].c_en         = false;
	st->sensor[SENSOR_GYRO].c_en          = false;
	st->sensor[SENSOR_COMPASS].c_en       = true;
	st->sensor[SENSOR_ALS].c_en           = false;
	st->sensor[SENSOR_PRESSURE].c_en      = false;
	st->sensor[SENSOR_SIXQ].c_en          = false;
	st->sensor[SENSOR_PEDQ].c_en          = false;
	st->sensor[SENSOR_CALIB_GYRO].c_en    = false;

	st->sensor[SENSOR_ACCEL].p_en         = false;
	st->sensor[SENSOR_GYRO].p_en          = false;
	st->sensor[SENSOR_COMPASS].p_en       = false;
	st->sensor[SENSOR_ALS].p_en           = false;
	st->sensor[SENSOR_PRESSURE].p_en      = true;
	st->sensor[SENSOR_SIXQ].p_en          = false;
	st->sensor[SENSOR_PEDQ].p_en          = false;
	st->sensor[SENSOR_CALIB_GYRO].p_en    = false;

	st->sensor[SENSOR_ACCEL].engine_base         = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_COMPASS].engine_base       = ENGINE_I2C;
	st->sensor[SENSOR_ALS].engine_base           = ENGINE_I2C;
	st->sensor[SENSOR_PRESSURE].engine_base      = ENGINE_I2C;
	st->sensor[SENSOR_SIXQ].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_PEDQ].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_CALIB_GYRO].engine_base    = ENGINE_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_SIXQ].base = SENSOR_SIXQ;
	st->sensor_l[SENSOR_L_PEDQ].base = SENSOR_PEDQ;
	st->sensor_l[SENSOR_L_PRESSURE].base = SENSOR_PRESSURE;
	st->sensor_l[SENSOR_L_ALS].base = SENSOR_ALS;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG_WAKE].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].base = SENSOR_PRESSURE;
	st->sensor_l[SENSOR_L_ALS_WAKE].base = SENSOR_ALS;

	st->sensor_l[SENSOR_L_ACCEL].header         = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GYRO].header          = GYRO_HDR;
	st->sensor_l[SENSOR_L_MAG].header           = COMPASS_HDR;
	st->sensor_l[SENSOR_L_SIXQ].header          = SIXQUAT_HDR;
	st->sensor_l[SENSOR_L_PEDQ].header          = PEDQUAT_HDR;
	st->sensor_l[SENSOR_L_PRESSURE].header      = PRESSURE_HDR;
	st->sensor_l[SENSOR_L_ALS].header           = ALS_HDR;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].header    = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_WAKE].header     = GYRO_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_WAKE].header      = COMPASS_WAKE_HDR;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].header = PRESSURE_WAKE_HDR;
	st->sensor_l[SENSOR_L_ALS_WAKE].header      = ALS_WAKE_HDR;

	st->sensor_l[SENSOR_L_ACCEL].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_MAG].wake_on = false;
	st->sensor_l[SENSOR_L_SIXQ].wake_on          = false;
	st->sensor_l[SENSOR_L_PEDQ].wake_on          = false;
	st->sensor_l[SENSOR_L_PRESSURE].wake_on      = false;
	st->sensor_l[SENSOR_L_ALS].wake_on           = false;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_MAG_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].wake_on      = true;
	st->sensor_l[SENSOR_L_ALS_WAKE].wake_on           = true;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].sample_size =
							ACCEL_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].sample_size  =
							GYRO_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_COMPASS_CALIB].sample_size  =
							CPASS_CALIBR_DATA_SZ;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].output = ACCEL_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].output = GYRO_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_COMPASS_CALIB].output  =
							COMPASS_CAL_INPUT_SET;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->firmware = 0;

	inv_init_secondary(st);
	inv_init_sensor_struct(st);

	res = inv_read_timebase(st);
	if (res)
		return res;
	res = inv_set_dmp(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;

	res = inv_set_secondary(st);

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	return res;
}

int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;
	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;

	result = inv_set_power(st, true);
	if (result)
		return result;
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_init_config(st);
	if (result)
		return result;

	if (SECONDARY_SLAVE_TYPE_COMPASS == plat->sec_slave_type)
		st->chip_config.has_compass = 1;
	else
		st->chip_config.has_compass = 0;
	if (SECONDARY_SLAVE_TYPE_PRESSURE == plat->aux_slave_type)
		st->chip_config.has_pressure = 1;
	else
		st->chip_config.has_pressure = 0;
	if (SECONDARY_SLAVE_TYPE_ALS == plat->read_only_slave_type)
		st->chip_config.has_als = 1;
	else
		st->chip_config.has_als = 0;

#if 0
st->plat_data.sec_slave_id = COMPASS_ID_AK09912;
 st->plat_data.secondary_i2c_addr  = 0xE;
st->plat_data.aux_slave_id = PRESSURE_ID_BMP280;
st->plat_data.aux_i2c_addr = 0x77;
st->plat_data.read_only_slave_id = ALS_ID_APDS_9930;
st->plat_data.read_only_i2c_addr = 0x39;
st->chip_config.has_compass = 1;
st->chip_config.has_pressure = 1;
st->chip_config.has_als = 1;
#endif
	if (st->chip_config.has_compass) {
		result = inv_mpu_setup_compass_slave(st);
		if (result) {
			pr_err("compass setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}
	if (st->chip_config.has_pressure) {
		result = inv_mpu_setup_pressure_slave(st);
		if (result) {
			pr_err("pressure setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}
	if (st->chip_config.has_als) {
		result = inv_mpu_setup_als_slave(st);
		if (result) {
			pr_err("als setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}

	result = mem_r(MPU_SOFT_REV_ADDR, 1, &v);
	if (result)
		return result;
	if (v & MPU_SOFT_REV_MASK) {
		pr_err("incorrect software revision=%x\n", v);
		return -EINVAL;
	} else {
		if (v == SW_REV_LP_EN_MODE)
			st->chip_config.lp_en_mode_off = 0;
		else
			st->chip_config.lp_en_mode_off = 1;
	}

	result = inv_set_power(st, false);

	return result;
}
