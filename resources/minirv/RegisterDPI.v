module RegisterDPI (
    input clock,
    input reset,
    input [31:0] RF_value_0,
    input [31:0] RF_value_1,
    input [31:0] RF_value_2,
    input [31:0] RF_value_3,
    input [31:0] RF_value_4,
    input [31:0] RF_value_5,
    input [31:0] RF_value_6,
    input [31:0] RF_value_7,
    input [31:0] RF_value_8,
    input [31:0] RF_value_9,
    input [31:0] RF_value_10,
    input [31:0] RF_value_11,
    input [31:0] RF_value_12,
    input [31:0] RF_value_13,
    input [31:0] RF_value_14,
    input [31:0] RF_value_15,
    input [31:0] RF_value_16,
    input [31:0] RF_value_17,
    input [31:0] RF_value_18,
    input [31:0] RF_value_19,
    input [31:0] RF_value_20,
    input [31:0] RF_value_21,
    input [31:0] RF_value_22,
    input [31:0] RF_value_23,
    input [31:0] RF_value_24,
    input [31:0] RF_value_25,
    input [31:0] RF_value_26,
    input [31:0] RF_value_27,
    input [31:0] RF_value_28,
    input [31:0] RF_value_29,
    input [31:0] RF_value_30,
    input [31:0] RF_value_31
);
    import "DPI-C" function void reg_get(input int i, input int reg_val);

    always @(posedge clock) begin
        if (!reset) begin
            reg_get(0,  int'(RF_value_0));
            reg_get(1,  int'(RF_value_1));
            reg_get(2,  int'(RF_value_2));
            reg_get(3,  int'(RF_value_3));
            reg_get(4,  int'(RF_value_4));
            reg_get(5,  int'(RF_value_5));
            reg_get(6,  int'(RF_value_6));
            reg_get(7,  int'(RF_value_7));
            reg_get(8,  int'(RF_value_8));
            reg_get(9,  int'(RF_value_9));
            reg_get(10, int'(RF_value_10));
            reg_get(11, int'(RF_value_11));
            reg_get(12, int'(RF_value_12));
            reg_get(13, int'(RF_value_13));
            reg_get(14, int'(RF_value_14));
            reg_get(15, int'(RF_value_15));
            reg_get(16, int'(RF_value_16));
            reg_get(17, int'(RF_value_17));
            reg_get(18, int'(RF_value_18));
            reg_get(19, int'(RF_value_19));
            reg_get(20, int'(RF_value_20));
            reg_get(21, int'(RF_value_21));
            reg_get(22, int'(RF_value_22));
            reg_get(23, int'(RF_value_23));
            reg_get(24, int'(RF_value_24));
            reg_get(25, int'(RF_value_25));
            reg_get(26, int'(RF_value_26));
            reg_get(27, int'(RF_value_27));
            reg_get(28, int'(RF_value_28));
            reg_get(29, int'(RF_value_29));
            reg_get(30, int'(RF_value_30));
            reg_get(31, int'(RF_value_31));
        end
    end
endmodule
