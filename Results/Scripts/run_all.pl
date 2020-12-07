#!/usr/bin/perl

$Global_Stat_Dir = "./Results/Stats";
unless (-e $Global_Stat_Dir or mkdir $Global_Stat_Dir) {
	die "Unable to create Stats dir\n";
}


for $bench("sphinx3") {

    #for $rp("LRU", "HAWKEYE") {
    for $rp("LRU") {

        $Current_Stat_Dir = "./Results/Stats/${bench}_${rp}";
        unless (-e $Current_Stat_Dir or mkdir $Current_Stat_Dir) {
        	die "Unable to create Stats dir\n";
        }

        $run_cmd = "./build/X86/gem5.opt".
		   " -d $Current_Stat_Dir".
                   " configs/spec2k6/run.py".
                   " -b $bench".
                   " --caches".
		   " --l2cache".
                   " --l3cache".
                   " --l3_replacement_policy=${rp}".
                   " --fast-forward=1000000000".
                   " --warmup-insts=100000000".
		   " --standard-switch=100000000".
                   " --maxinsts=1000000000";  


	system($run_cmd);
    }
}
