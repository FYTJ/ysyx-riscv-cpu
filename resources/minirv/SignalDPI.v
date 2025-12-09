module SignalDPI #(
    parameter int WIDTH = 32,
    parameter int ID = 0,
    parameter string NAME = ""
) (
    input clock,
    input reset,
    input [WIDTH-1:0] in
);

    import "DPI-C" function void npc_signal_register(input string name, input int width, input int id);
    import "DPI-C" function void npc_signal_update(input int id, input int value);

    initial begin
        npc_signal_register(NAME, WIDTH, ID);
    end

    always @(posedge clock) begin
        npc_signal_update(ID, int'(in));
    end
endmodule
