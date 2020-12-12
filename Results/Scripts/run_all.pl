#!/usr/bin/perl

$debug_mode = 0;

$Global_Stat_Dir = "./Results/Stats_Backup";
unless (-e $Global_Stat_Dir or mkdir $Global_Stat_Dir) {
	die "Unable to create Stats dir\n";
}


#for $bench("sphinx3", "cactusADM", "GemsFDTD", "soplex", "mcf", "xalancbmk", "hmmer", "tonto", "h264ref", "bzip2", "astar", "gromacs") {
for $bench("gromacs") {
    for $rp("BRRIP") {

        $Current_Stat_Dir = "$Global_Stat_Dir/${bench}_${rp}";
        unless (-e $Current_Stat_Dir or mkdir $Current_Stat_Dir) {
        	die "Unable to create Stats dir\n";
        }

        $gem5_cmd = "./build/X86/gem5.opt".
		   " -d $Current_Stat_Dir";

        $config_cmd = " configs/spec2k6/run.py".
                   " -b $bench".
                   " --caches".
		   " --l2cache".
                   " --l3cache".
                   " --l3_replacement_policy=${rp}";


        if ($debug_mode==0) {
                $config_cmd = $config_cmd .
                   " --fast-forward=1000000000".
                   " --warmup-insts=100000000".
		   " --standard-switch=100000000".
                   " --maxinsts=1000000000";  
	}	
	if ($debug_mode==1) {
		#$config_cmd = $config_cmd." --cpu-type=DerivO3CPU".
		#		      " --maxinsts=250000";

                $config_cmd = $config_cmd .
                   " --fast-forward=100000000".
                   " --warmup-insts=10000000".
		   " --standard-switch=100000000".
                   " --maxinsts=500000000";  
                #$gem5_cmd = $gem5_cmd . " --debug-flags=CacheRepl";

	}

        $run_cmd = $gem5_cmd. $config_cmd;

	system($run_cmd);
    }
}
