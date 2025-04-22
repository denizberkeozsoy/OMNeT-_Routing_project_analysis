package node;


simple Routing
{
    parameters:
        // ✅ CHANGE: Added parameter for centralized routing
        // ✔️ IMPACT: Allows enabling/disabling centralized route computation via ini file
        bool centralRouting = default(false);  

        @display("i=block/switch");
        @signal[drop](type="long");
        @signal[outputIf](type="long");

        @statistic[drop](title="dropped packet byte length";unit=bytes;record=vector?,count,sum;interpolationmode=none);
        @statistic[outputIf](title="output interface for each routed packet";record=histogram;interpolationmode=none);
    gates:
        input in[];
        output out[];
        input localIn;
        output localOut;
}
