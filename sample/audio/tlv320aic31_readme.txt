1.It need to enable I2C3 and I2S0. The GPIO of mipi is shared with the I2c3 function GPIO, and "mipi_tx_set_rest" need to be commented out.
  update the file: drv/interdrv/sysconfig/sys_config.c

  int pinmux(int cmos_yuv_flag)
  {
      ......
      i2c3_pin_mux();
      i2c3_reset();
      ......
      //mipi_tx_set_rest();
      ......
      i2s0_pin_mux();
      ......
  }

2. update the file: mpp/ko/load3516dv300
   It need to insmod tlv_320aic31's driver.
    insmod extdrv/hi_tlv320aic31.ko

3. modify the makefile parameter: mpp/sample/Makefile.param. Set ACODEC_TYPE to  ACODEC_TYPE_TLV320AIC31.
   It means use the external codec tlv_320aic31 sample code.
    ################ select audio codec type for your sample ################
    #ACODEC_TYPE ?= ACODEC_TYPE_INNER
    #external acodec
    ACODEC_TYPE ?= ACODEC_TYPE_TLV320AIC31

4. Rebuild the sample and get the sample_audio.
