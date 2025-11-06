module MemoryDPI(
    input clock,
    input reset,
    input ren,
    input wen,
    input [31:0] addr,
    input [3:0] wstrb,
    input [31:0] wdata,
    output reg [31:0] rdata,
    output reg wfinish
);

    import "DPI-C" function int mem_read_word(input int addr);
    import "DPI-C" function int mem_write_word(input int addr, input int data, input byte strb);
    import "DPI-C" function void mem_init();

    initial begin
        mem_init();
    end

    always @(posedge clock) begin
        if (reset) begin
            rdata <= 32'b0;
        end 
        else if (ren) begin
            rdata <= mem_read_word(addr);
        end
    end

    always @(posedge clock) begin
        if (reset) begin
            wfinish <= 0;
        end 
        else if (wen) begin
            wfinish <= (mem_write_word(addr, wdata, {4'b0, wstrb}) == 1);
        end 
    end
endmodule
