#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/gfp.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <mach/mt_typedefs.h>
#include <mach/sbchk_base.h>

/**************************************************************************
 *  MODULE DEFINITION
 **************************************************************************/
#define MOD                         "SBCHK_BASE"
#define KER_SHA1_TEST               (0)

/**************************************************************************
 *  MODULE MACRO
 **************************************************************************/
#ifndef ASSERT
#define ASSERT(expr)            BUG_ON(!(expr))
#endif

/**************************************************************************
 *  GLOBAL VARIABLES
 **************************************************************************/
bool bIsChecked = FALSE;

/**************************************************************************
 *  UTILITIES
 **************************************************************************/
void sbchk_dump(unsigned char *buf, unsigned int len)
{
	unsigned int i = 1;

	for (i = 1; i < len + 1; i++)
		pr_info("%02x", buf[i - 1]);

	pr_info("\n");
}

void sbchk_hex_string(unsigned char *buf, unsigned int len)
{
	unsigned int i = 1;

	for (i = 1; i < len + 1; i++)
		pr_info("%c", buf[i - 1]);

	pr_info("\n");
}

/**************************************************************************
 *  KERNEL SHA1 FUNCTION
 **************************************************************************/
unsigned int sbchk_sha1(char *code, unsigned int code_len, char *result)
{
	unsigned int ret = SEC_OK;
	struct scatterlist sg[1];
	struct crypto_hash *tfm = NULL;
	struct hash_desc desc;

	tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		ret = SBCHK_BASE_HASH_INIT_FAIL;
		goto _exit;
	}

	/* sg_init_one(&sg[0], plaintext, length); */
	sg_set_buf(&sg[0], code, code_len);

	desc.tfm = tfm;
	desc.flags = 0;
	memset(result, 0, 20);	/* SHA1 returns 20 bytes */
	if (crypto_hash_digest(&desc, sg, code_len, result)) {
		ret = SBCHK_BASE_HASH_DATA_FAIL;
		goto _exit;
	}

	crypto_free_hash(tfm);

 _exit:

	return ret;
}

/**************************************************************************
 *  KERNEL SHA1 TEST
 **************************************************************************/
void sbchk_test(void)
{
	char *code1 = "2ew34123132513451345";
	char *code2 = "234123132513451345";
	char *code3 = "2ew34123132513451345";
	char *hash_rs1, *hash_rs2, *hash_rs3;

	hash_rs1 = kmalloc(HASH_OUTPUT_LEN, GFP_KERNEL);
	hash_rs2 = kmalloc(HASH_OUTPUT_LEN, GFP_KERNEL);
	hash_rs3 = kmalloc(HASH_OUTPUT_LEN, GFP_KERNEL);

	sbchk_sha1(code1, strlen(code1), hash_rs1);
	sbchk_sha1(code2, strlen(code2), hash_rs2);
	sbchk_sha1(code3, strlen(code3), hash_rs3);

	pr_info("[%s] dump result 1:\n", MOD);
	sbchk_dump(hash_rs1, HASH_OUTPUT_LEN);
	pr_info("[%s] dump result 2:\n", MOD);
	sbchk_dump(hash_rs2, HASH_OUTPUT_LEN);
	pr_info("[%s] dump result 3:\n", MOD);
	sbchk_dump(hash_rs3, HASH_OUTPUT_LEN);

	if (memcmp(hash_rs1, hash_rs2, HASH_OUTPUT_LEN) != 0) {
		pr_info("[%s] <1>code1 != code2. TEST PASS\n", MOD);
	} else {
		pr_info("[%s] <1>code1 == code2. TEST FAIL (KERNEL SHA1 FAIL)\n", MOD);
		ASSERT(0);
	}

	if (memcmp(hash_rs1, hash_rs3, HASH_OUTPUT_LEN) != 0) {
		pr_info("[%s] <1>code1 != code3. TEST FAIL (KERNEL SHA1 FAIL)\n", MOD);
		ASSERT(0);
	} else {
		pr_info("[%s] <1>code1 == code3. TEST PASS\n", MOD);
	}

	kfree(hash_rs1);
	kfree(hash_rs2);
	kfree(hash_rs3);
}

/**************************************************************************
 *  CHECK FILE HASH
 **************************************************************************/
unsigned int sbchk_verify(char *file_path, char *hash_val)
{
	struct file *fd;
	unsigned int ret = SEC_OK;
	unsigned int file_size = 0;
	char *hash_rs = NULL;
	char *buf = NULL;
	bool bBufAllocated = FALSE;
	struct inode *inode;

	/* save current file system type */
	mm_segment_t fs = get_fs();

	/* ----------------------- */
	/* open security file      */
	/* ----------------------- */
	/* open engine */
	fd = filp_open(file_path, O_RDONLY, 0);
	if (fd < 0) {
		pr_info("[%s] Open '%s' fail\n", MOD, file_path);
		ret = SBCHK_BASE_ENGINE_OPEN_FAIL;
		goto _end;
	}

	/* ----------------------- */
	/* configure file system   */
	/* ----------------------- */
	set_fs(KERNEL_DS);

	/* ----------------------- */
	/* allocate buffer         */
	/* ----------------------- */
	inode = fd->f_dentry->d_inode;
	file_size = inode->i_size;
	pr_info("[%s] '%s' exists ('%d' byets)\n", MOD, file_path, file_size);
	buf = kmalloc(file_size, GFP_KERNEL);
	hash_rs = kmalloc(HASH_OUTPUT_LEN, GFP_KERNEL);
	bBufAllocated = TRUE;

	/* ----------------------- */
	/* read security file      */
	/* ----------------------- */
	/* read image to input buffer */
	file_size = fd->f_op->read(fd, buf, file_size, &fd->f_pos);
	if (0 >= file_size) {
		ret = SBCHK_BASE_ENGINE_READ_FAIL;
		pr_info("[%s] Read '%s' '%d' byets fail\n", MOD, file_path, file_size);
		goto _end;
	}

	pr_info("[%s] Read '%s' '%d' byets\n", MOD, file_path, file_size);

	/* ----------------------- */
	/* calculate hash          */
	/* ----------------------- */
	sbchk_sha1(buf, file_size, hash_rs);
	pr_info("[%s] Calculate the hash value of '%s' =\n", MOD, file_path);
	sbchk_dump(hash_rs, HASH_OUTPUT_LEN);


	/* ----------------------- */
	/* verify hash             */
	/* ----------------------- */
#if SBCHK_ENGINE_HASH_CHECK
	{
		unsigned int i = 0;
		char hash_rsn[HASH_OUTPUT_LEN * 2 + 1] = { 0 };
		char *hash_prsn = hash_rsn;


		/* convert hash value to 'hex' string */
		for (i = 0; i < HASH_OUTPUT_LEN; i++) {
			sprintf(hash_prsn, "%02x", hash_rs[i]);
			hash_prsn += 2;
		}


		/* compare hash value */
		if (memcmp(hash_rsn, hash_val, HASH_OUTPUT_LEN) != 0) {
			pr_info("[%s] Hash check fail. The value should be\n", MOD);
			sbchk_hex_string(hash_val, HASH_OUTPUT_LEN * 2);
			ret = SBCHK_BASE_HASH_CHECK_FAIL;
			goto _end;
		} else {
			pr_info("[%s] Hash check pass\n", MOD);
			sbchk_hex_string(hash_val, HASH_OUTPUT_LEN * 2);
		}
	}
#endif


 _end:

	set_fs(fs);
	if (TRUE == bBufAllocated) {
		kfree(hash_rs);
		kfree(buf);
	}
	return ret;
}

/**************************************************************************
 *  KERNEL SBCHK
 **************************************************************************/
void sbchk_base(void)
{

#ifdef CONFIG_SBCHK_BASE_ENABLE

	unsigned int ret = SEC_OK;

	/* --------------------------------- */
	/* verify security file              */
	/* --------------------------------- */
	if (FALSE == bIsChecked) {
		bIsChecked = TRUE;
		pr_info("[%s] Enter\n", MOD);

		/* --------------------------------- */
		/* test sbchk_sha1                   */
		/* --------------------------------- */
#if KER_SHA1_TEST
		sbchk_test();
#endif

		/* --------------------------------- */
		/* verify user space security engine */
		/* --------------------------------- */
		ret = sbchk_verify(SBCHK_ENGINE_PATH, SBCHK_ENGINE_HASH);
		if (SEC_OK != ret) {
			msleep(20000);
			pr_info("[%s] Verify '%s' fail. ret '%x'\n", MOD, SBCHK_ENGINE_PATH, ret);
			/* punishment can be customized */
			ASSERT(0);
		}

		/* --------------------------------- */
		/* verify kernel security module     */
		/* --------------------------------- */
		ret = sbchk_verify(SBCHK_MODULE_PATH, SBCHK_MODULE_HASH);
		if (SEC_OK != ret) {
			msleep(20000);
			pr_info("[%s] Verify '%s' fail. ret '%x'\n", MOD, SBCHK_MODULE_PATH, ret);
			/* punishment can be customized */
			ASSERT(0);
		}

		/* --------------------------------- */
		/* verify kernel core modem module   */
		/* --------------------------------- */
		ret = sbchk_verify(MODEM_CORE_MODULE_PATH, MODEM_CORE_MODULE_HASH);
		if (SEC_OK != ret) {
			msleep(20000);
			pr_info("[%s] Verify '%s' fail. ret '%x'\n", MOD, MODEM_CORE_MODULE_PATH,
				ret);
			/* punishment can be customized */
			ASSERT(0);
		}

		/* --------------------------------- */
		/* verify kernel plat modem module   */
		/* --------------------------------- */
		ret = sbchk_verify(MODEM_PLAT_MODULE_PATH, MODEM_PLAT_MODULE_HASH);
		if (SEC_OK != ret) {
			msleep(20000);
			pr_info("[%s] Verify '%s' fail. ret '%x'\n", MOD, MODEM_PLAT_MODULE_PATH,
				ret);
			/* punishment can be customized */
			ASSERT(0);
		}
#if 0
		/* --------------------------------- */
		/* verify init rc                    */
		/* --------------------------------- */
		ret = sbchk_verify(INIT_RC_PATH, INIT_RC_HASH);
		if (SEC_OK != ret) {
			msleep(20000);
			pr_info("[%s] Verify '%s' fail. ret '%x'\n", MOD, INIT_RC_PATH, ret);
			/* punishment can be customized */
			ASSERT(0);
		}
#endif
	}
#endif

}

/**************************************************************************
 *  GET devinfo info with index
 **************************************************************************/
u32 get_devinfo_with_index(u32 index)
{
	int size = (sizeof(g_devinfo_data) / sizeof(u32));
	if ((index >= 0) && (index < size)) {
		return g_devinfo_data[index];
	} else {
		pr_info("devinfo data index out of range:%d\n", index);
		pr_info("devinfo data size:%d\n", size);
		return SBCHK_BASE_INDEX_OUT_OF_RANGE;
	}
}
