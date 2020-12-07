#!/usr/bin/perl

$Global_Ckpt_Dir = "./Results/Checkpoints";
unless (-e $Global_Ckpt_Dir or mkdir $Global_Ckpt_Dir) {
	die "Unable to create Checkpoints dir\n";
}

$Global_CkptLog_Dir = "./Results/Checkpoints/Logs";
unless (-e $Global_CkptLog_Dir or mkdir $Global_CkptLog_Dir) {
	die "Unable to create Checkpoints Log dir\n";
}

$Global_Simpoint_Dir = "./Results/Simpoints";

for $bench("bzip2") {

    for $rp("LRU", "HAWKEYE") {

        $Current_Ckpt_Dir = "${Global_Ckpt_Dir}/${bench}_${rp}";
        unless (-e $Current_Ckpt_Dir or mkdir $Current_Ckpt_Dir) {
        	die "Unable to create Ckpt dir for $bench with $rp\n";
        }
        
        $Current_CkptLog_Dir = "${Global_CkptLog_Dir}/${bench}_${rp}";
        unless (-e $Current_CkptLog_Dir or mkdir $Current_CkptLog_Dir) {
        	die "Unable to create CkptLog dir for $bench with $rp\n";
        }

        $simpoint_file_path = "$Global_Simpoint_Dir/$bench/simpoint_file";
        $weight_file_path = "$Global_Simpoint_Dir/$bench/weight_file";
        $interval_length = 250000000; #250M
        $warmup_length = 50000000; #50M

        $run_cmd = "./build/X86/gem5.opt".
		   " -d $Current_CkptLog_Dir".
                   " configs/spec2k6/run.py".
                   " -b $bench".
                   " --cpu-type=AtomicSimpleCPU".
                   " --caches".
		   " --l2cache".
                   " --l3cache".
                   " --l3_replacement_policy=${rp}".
                   " --take-simpoint-checkpoint=${simpoint_file_path},${weight_file_path},${interval_length},${warmup_length}".
                   " --checkpoint-dir=$Current_Ckpt_Dir";

	system($run_cmd);
    }
}
