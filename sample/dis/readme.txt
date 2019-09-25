Before using the gyro sensor,the config of kernel and some files should be modified firstly.

1.Modify the clock-frequency of i2c_bus11 from 100000 to 400000.
  The clock-frequency of i2c_bus11 is in the file "linux-3.18.y/arch/arm64/boot/dts/hisilicon/hi3519av100.dtsi".
   
i2c_bus7: i2c@12117000 {
        compatible = "hisilicon,hibvt-i2c";
        reg = <0x12117000 0x1000>;
        clocks = <&clock HI3519AV100_I2C11_CLK>;
        clock-frequency = <400000>;
        status = "disabled";
};
