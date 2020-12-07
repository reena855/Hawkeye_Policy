#!/usr/bin/perl

$Global_Ckpt_Dir = "./Results/Checkpoints";

$Global_Stat_Dir = "./Results/Simpoint_Stats";
unless (-e $Global_Stat_Dir or mkdir $Global_Stat_Dir) {
	die "Unable to create Simpoint_Stats dir\n";
}

$Global_Simpoint_Dir = "./Results/Simpoints";

for $bench("bzip2") {

    for $rp("LRU", "HAWKEYE") {

        $Current_Ckpt_Dir = "${Global_Ckpt_Dir}/${bench}_${rp}";
        
        $Current_Stat_Dir = "${Global_Stat_Dir}/${bench}_${rp}";
        unless (-e $Current_Stat_Dir or mkdir $Current_Stat_Dir) {
        	die "Unable to create Stat dir for $bench with $rp\n";
        }

        $run_cmd = "./build/X86/gem5.opt".
		   " -d $Current_Stat_Dir".
                   " configs/spec2k6/run.py".
                   " -b $bench".
                   " --restore-simpoint-checkpoint".
		   " -r 4".
                   " --cpu-type=TimingSimpleCPU".
                   " --restore-with-cpu=DerivO3CPU".
                   " --caches".
		   " --l2cache".
                   " --l3cache".
                   " --l3_replacement_policy=${rp}".
                   " --checkpoint-dir=$Current_Ckpt_Dir";

	system($run_cmd);
    }
}
