#!/usr/bin/perl
# 
# Copyright Statement:
# --------------------
# This software is protected by Copyright and the information contained
# herein is confidential. The software may not be copied and the information
# contained herein may not be used or disclosed except with the written
# permission of MediaTek Inc. (C) 2010
# 
# BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
# NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
# SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
# 
# BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
# LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
# 
# THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
# WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
# LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
# RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
# THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
# 
use Sys::Hostname;
my $DEBUG = 0; # enable/disable script debugging message

# Set environment variable for current Android version building
# such as, JDK version, arm-eabi version etc.
# If anymore, you can specify them in ./mbldenv.sh
setBuildEnvVars("./mbldenv.sh");
setCacheEnvVars() if (uc($ENV{"MTK_INTERNAL"}) eq "TRUE");
# End

$myCmd = "makeMtk"; #$0

# BEGIN xiazs1, make symbolic link to android/out
if (!-e "../bsp/out")
{
  symlink("../android/out", "../bsp/out");
}
# END

#space check for build path
$result = `pwd`;
@number = split(" ",$result);
die "******************************************************************************  \
You are building in a directory whose absolute path contains a space character: \
$result \
Please move your source tree to a path that does not contain any spaces.        \
******************************************************************************  \
Directory names containing spaces not supported\n" if($#number != 0);
@keywords = ('\[','\]','\(','\)','\{','\}');
foreach $k (@keywords)
{
@number = split($k,$result);
die "******************************************************************************  \
You are building in a directory whose absolute path contains a special character: \
$result \
Please move your source tree to a path that does not contain \"$k\".        \
******************************************************************************  \
Directory names containing spaces not supported\n" if($#number != 0);
}
$performanceChk = 0;
($performanceChk == 1) && print &CurrTimeStr . " Enter $myCmd\n";

@chkDirs = qw(mediatek mediatek/config mediatek/build/tools);
foreach $m (@chkDirs)
{
  die "Folder \"$m\" does NOT exist!\nPlease help confirm Mediatek release package.\n" if (!-d $m);
}

@arguments = ();
my $product = "";
$project = "";
$action = "";
@mOpts = ();
$ini = "makeMtk.ini";
$enINI = (-e $ini) ? 1 : 0;
(($#ARGV < 0) || (($#ARGV < 1) && (($enINI == 0) && (lc($ARGV[0]) !~ /^[listp|banyan_addon|banyan_addon_x86]/)))) && &Usage;

$tee = "FALSE";

# mapping table between flag in project name and corresponding product
my $products = 
{
  "phone"   => "phone",
  "ph"      => "phone",
  "tablet"  => "tablet",
  "tb"      => "tablet",
  "tv"      => "tv",
  "default" => "phone"
};
#lenovo-sw jixj 2013.10.1 modified begin
#@actions = 
#qw(
#   new n bm_new remake r bm_remake clean c listproject listp
#   drvgen codegen emigen nandgen custgen javaoptgen ptgen run-preprocess remove-preprocessed
#   check-modem update-modem sign-image encrypt-image sign-modem check-dep
#   dump-memusage gen-relkey check-appres
#   rel-cust modem-info bindergen mrproper pregen
#  );
@actions = 
qw(
   new n bm_new remake r bm_remake clean c listproject listp
   drvgen codegen emigen nandgen custgen javaoptgen ptgen run-preprocess remove-preprocessed
   check-modem update-modem sign-image encrypt-image sign-modem check-dep
   dump-memusage gen-relkey check-appres
   rel-cust modem-info bindergen mrproper pregen mtxgen
  );
#lenovo-sw jixj 2013.10.1 modified begin

my @ABSNativeActions = 
qw(
   preloadimage customimage bootimage systemimage recoveryimage secroimage cacheimage factoryimage
   userdataimage userdataimage-nodeps target-files-package
   sdk win_sdk banyan_addon banyan_addon_x86 cts otapackage dist updatepackage
   update-api mm mma snod dump-comp-build-info dump-products bootimage-nodeps
   ramdisk-nodeps
  );

push(@actions, @ABSNativeActions);

@modules = qw(android kernel dr k preloader pl lk tz md32);
@orgARGV = @ARGV;

while ($#ARGV != -1)
{
  if ($ARGV[0] =~ /^-(t|te|tee)/)
  {
    $tee = "TRUE";
  }
  elsif ($ARGV[0] =~ /^-h|help/) 
  {
    &Usage;
  }
  elsif ($ARGV[0] =~ /^-(o|op|opt)=(.*)$/)
  {
    @mOpts = split(",", $2);
  }
  elsif ($ARGV[0] =~ /^-smart$/)
  {
    unshift(@mOpts, "MTK_DEPENDENCY_AUTO_CHECK=true");
  }
  elsif ($ARGV[0] =~ /^(listp|listproject)/)
  {
    &p_system("perl mediatek/build/tools/listP.pl");
    exit 0;
  }
  elsif ($ARGV[0] =~ /check-env|chk-env/)
  {
    &chkMustEnv;
    exit 0;
  } 
  else
  {
    $project = lc($ARGV[0]);
    $project = "generic" if ($project eq "emulator");

    # [heqi1]: Call lenovo envsetup.sh
    my $envVarList = `bash ./build/lenovo_envsetup.sh $project | grep ".*=.*"`;
    map
    {
      chomp;
      $ENV{$1}=$2 if (/(.*)=(.*)/);
    } split(/\n/, $envVarList);
    # [heqi1] end

    if (!-e "mediatek/config/${project}/ProjectConfig.mk")
    {
      if (-e $ini)
      {
        open (FILE_HANDLE, "<$ini") or die "cannot open $ini\n";
        while (<FILE_HANDLE>)
        {
          if (/^(\S+)\s*=\s*(\S+)/)
          {
            $keyname = $1;
            $${keyname} = $2;
          }
        }
        close FILE_HANDLE;
        $project = lc($project);
        die "Can NOT find project Makefile "
            . "mediatek/config/${project}/ProjectConfig.mk from $ini\n"
        if (!-e "mediatek/config/${project}/ProjectConfig.mk");
      }
    }
    else
    {
      shift(@ARGV);
    }

    die "Can NOT recognize project Makefile "
        . "mediatek/config/${project}/ProjectConfig.mk from command\n"
    if (!-e "mediatek/config/${project}/ProjectConfig.mk");
    $action = lc($ARGV[0]);
    shift(@ARGV);
    @arguments = @ARGV;
    @ARGV = ();
  }
  shift(@ARGV);
}

foreach $prj ("mediatek/config/${project}/ProjectConfig.mk", "mediatek/config/common/ProjectConfig.mk")
{
  open (FILE_HANDLE, "<$prj") or die "cannot open $prj\n";
  while (<FILE_HANDLE>)
  {
    if (/^(\S+)\s*=\s*(\S+)/)
    {
      $ENV{$1} = $2;
    }
  }
  close FILE_HANDLE;
}


my $flavor = "";
if ( $project =~ /\[(.*)\]/ )
{
  $flavor = $1;
  $project =~ s/\[(.*)\]//;
}

if ($project =~ /^(tablet)/ || $project =~ /_(ph|tb|tv|phone|tablet)$/)
{
  my $key = (defined $1)? $1 : "default";
  $product = $products->{$key};
}

if (($project ne "banyan_addon") && ($project ne "banyan_addon_x86")) {
  die "Lack off the action.\nIt should be one of (@actions)\nOr try $myCmd -h\n" if ($action eq "");
}

@acts = split(",", $action);
my $snod;
my $MM_PATH;
foreach $uAct (@acts)
{
  $isFound = 0;
  foreach $sAct (@actions)
  {
    if ($uAct eq $sAct)
    {
      $isFound = 1;
      last;
    }
  }
  die "Unknown the action: $uAct.\nPlease use $myCmd -h, if you need help!\n" if ($isFound == 0);
  if (($uAct eq "bm_remake") || ($uAct eq "bm_new"))
  {
    $uAct =~ s/^bm_//;
    push(@mOpts, "-k");
  }
  if (($uAct eq "mm")||($uAct eq "mma"))
  {
    foreach my $path (@arguments)
    {
      if ($path eq "snod")
      {
        $snod = $path;
      }
      else
      {
        die "the given $path does not exist!" if (! -d $path);
        $MM_PATH .= "\\ " if ($MM_PATH ne "");
        $MM_PATH .= $path;
      }
    }
    die "the target behind mm must be snod, not $snod" if (($snod ne "") && ($snod ne "snod"));
  }
  ($uAct = "new") if ($uAct eq "n");
  ($uAct = "clean") if ($uAct eq "c");
  ($uAct = "remake") if ($uAct eq "r");
  if (($uAct eq "remake") || ($uAct eq "bm_remake"))
  {
     unshift(@mOpts, "MTK_DEPENDENCY_AUTO_CHECK=true");
     if (-e $ini)
     {   
        open (FILE_HANDLE, "<$ini") or die "cannot open $ini\n";
        while (<FILE_HANDLE>)
        {   
          if (/^(\S+)\s*=\s*(\S+)/)
          {   
            $keyname = $1;
            if ($keyname eq "build_mode") {$${keyname} = $2;}
          }   
        }  
        foreach $i (@mOpts)
        {
          @temp = split("=","$i");
          if ($temp[0] eq "TARGET_BUILD_VARIANT")
          {
             $TARGET_BUILD_VARIANT = $temp[1];
          }
        }
        if ($TARGET_BUILD_VARIANT eq "") { $TARGET_BUILD_VARIANT = "eng"; }
        if ($build_mode ne $TARGET_BUILD_VARIANT) { print "WARNING *** : Previous build is $build_mode build, and now is $TARGET_BUILD_VARIANT build\n"; }  
     }
  }
}
&writeINI;
#print "@acts\n";

my $signSubStr;
if ($action eq "gen-relkey")
{
  if ($#arguments >= 0)
  {
    die "Wrong command line arguments, do NOT set any more arguments after 'gen-relkey'.\n"
      . "Please try $myCmd -h for help message.\n";
  }
  else
  {
    # get signature input
    $signSubStr = inputSignSubject();
    print "\nYour signature subject is '$signSubStr'\n";
  }
}
else
{
  my $i = 0;
  if ($#arguments >= 0)
  {
    foreach $uMod (@arguments)
    {
      $isFound = 0;
      foreach $sMod (@modules)
      {
        if ($uMod eq $sMod)
        {
          $isFound = 1;
          last;
        }
      }
      die "Unknown the module: $uMod.\n"
        . "It should be one of (@modules)\nOr try $myCmd -h\n"
      if ( ($isFound == 0) && ($arguments[$i - 1] !~ /[k|kernel|dr|android]/) );
      ($uMod = "preloader") if ($uMod eq "pl");
      ($uMod = "trustzone") if ($uMod eq "tz");
      ($uMod = "kernel") if ($uMod eq "k");  
      ($uMod = "android") if ($uMod eq "dr");
      if (($uMod eq "kernel") && ($arguments[$i + 1] =~ /^(.*)\//))
      {
        $KMOD_PATH = $arguments[$i + 1];
        $KMOD_PATH =~ s/^kernel\///;
        last;
      }
      if ($uMod eq "android")
      {
        $DR_MODULE = $arguments[$i + 1];
        last;
      }
      $i += $i + 1;
    }
  }
  else
  {
    @default_arguments = qw(preloader lk md32 tz kernel android);
  }
}

#logdir should sync with Makefile.yusu
my $out_dir = "out";
foreach my $i (@mOpts)
{
  my @temp = split("=",$i);
  if ($temp[0] eq "OUT_DIR")
  {
    $out_dir = $temp[1];
  }
}
$ENV{"MTK_PROJECT"} = $project;
my $actPttn = join('|', @actions);
if (join(' ', @acts) =~ /($actPttn)/ || $project eq "banyan_addon" || $project eq "banyan_addon_x86")
{
  ($performanceChk == 1) && print &CurrTimeStr . " run pwd\n";
  chomp($mktopdir = `pwd`);
  ($performanceChk == 1) && print &CurrTimeStr . " finish pwd\n";
  
  push(@mOpts, "ENABLE_TEE=$tee");
  push(@mOpts, "PROJECT=$project");
  push(@mOpts, "FLAVOR=$flavor");
  push(@mOpts, "MKTOPDIR=$mktopdir");
  push(@mOpts, "PRODUCT=$product");

  if ($action ne "gen-relkey")
  {
    $logdir = $out_dir . "/target/product";
    &p_system("mkdir -p $logdir") if (!-d $logdir);

    my $ABSActPttn = join('|',@ABSNativeActions);
    if (join(' ', @acts) =~ /($ABSActPttn)/)
    {
      foreach my $act (@acts)
      {
        &p_system("rm -f $logdir/${project}_${act}.log");
        &p_system("rm -f $logdir/${project}_${act}.log_err");
      }
    }
    if (join(' ', @acts) =~ /(new|remake|clean)/)
    {
      @create_arguments = ($#arguments < 0) ? @default_arguments : @arguments;
      foreach $uMod (@create_arguments)
      {
        #p_system("cat /dev/null > $logdir/${uMod}.log");
        &p_system("rm -f $logdir/${project}_${uMod}.log");
        &p_system("rm -f $logdir/${project}_${uMod}.log_err");
  #      &p_system("touch $logdir/${project}_${uMod}.log");
      }
    }
  }
}

$cmdArg = "CMD_ARGU=\"@mOpts\"";
$makeCmd = "make -f mediatek/build/makemtk.mk $cmdArg @mOpts";
#print "$makeCmd\n";

$result = 0;

# gen. release key/certificate
if ($action eq "gen-relkey")
{
  print "Start to generate release key/certificate for application signing...\n";
  $result += &p_system("$makeCmd $action SIGNATURE_SUBJECT=$signSubStr");
  (exit 255) if ($result >= 255);
  exit $result;
}

if($#acts == -1 && $project eq "banyan_addon")
{
 &chkMustEnv;
 &chkDep;
 $result += &p_system("$makeCmd banyan_addon");
}

if($#acts == -1 && $project eq "banyan_addon_x86")
{
 &chkMustEnv;
 &chkDep;
 $result += &p_system("$makeCmd banyan_addon_x86");
}

foreach $uAct (@acts)
{
  if (($uAct eq "codegen") || ($uAct eq "custgen"))
  {
    $result += &p_system("$makeCmd $uAct");
  }
  elsif (($uAct eq "mm")||($uAct eq "mma"))
  {
    $result += &p_system("$makeCmd $uAct SNOD=$snod MM_PATH=$MM_PATH");
  }
  else
  {
    if ($#arguments < 0)
    {
      if (${uAct} =~ /(new|remake|clean)/)
      {
        if ($uAct =~ /(new|remake)/)
        {
          &chkMustEnv;
          &chkDep;
        }
        $result += &p_system("$makeCmd ${uAct}all");
      }
      else
      {
        $result += &p_system("$makeCmd $uAct CUR_MODULE=android");
      }
    }
    else
    {
      foreach $uMod (@arguments)
      {
        &chkDep("$uMod");
        if ( ($KMOD_PATH ne "") && ($uMod eq "kernel") )
        {
          $result += &p_system("$makeCmd $uAct CUR_MODULE=$uMod KMOD_PATH=$KMOD_PATH");
          last;
        }
        elsif ( ($DR_MODULE ne "") && ($uMod eq "android") )
        {
          $result += &p_system("$makeCmd $uAct CUR_MODULE=$uMod DR_MODULE=$DR_MODULE");
          last;
        }
        else
        {
          $result += &p_system("$makeCmd $uAct CUR_MODULE=$uMod");
        }
      }
    }
  }
}  

($performanceChk == 1) && print &CurrTimeStr . " write ini\n";

($performanceChk == 1) && print &CurrTimeStr . " Exit $myCmd\n";

(exit 255) if ($result >= 255);
exit $result;

sub CurrTimeStr
{
  my($sec, $min, $hour, $mday, $mon, $year) = localtime(time);
  return (sprintf "%4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d", $year+1900, $mon+1, $mday, $hour, $min, $sec);
}

sub Usage
{
  warn << "__END_OF_USAGE";
Usage: (makeMtk|mk) [options] project actions [modules]

Options:
  -t, -tee      : Print log information on the standard-out.
  -o, -opt=bypass_argument_to_make
                : Pass extra arguments to make.

  -h, -help     : Print this message and exit.

Projects:
  one of available projects.

Actions:
  listp, listproject
                : List all available projects.

  check-env     : Check if build environment is ready.
  check-dep     : Check feature dependency.
  n, new        : Clean and perform a full build.
  c, clean      : Clean the immediate files(such as, objects, libraries etc.).
  r, remake     : Rebuild(target will be updated if any dependency updats).
  mrproper      : Remove all generated files + config + various backup files in Kbuild process.
  bm_new        : "new" + GNU make's "-k"(keep going when encounter error) feature.
  bm_remake     : "remake" + GNU make's "-k"(keep going when encounter error) feature.
  mm            : Build module through Android native command "mm"
  mma           : Build module through Android native command "mma"

  emigen        : Generate EMI setting source code.
  nandgen       : Generate supported NAND flash device list.
  codegen       : Generate trace DB(for META/Cather etc. tools used).
  drvgen        : Generate driver customization source.
  custgen       : Generate customization source.
  javaoptgen    : Generate the global java options.
  ptgen         : Generate partition setting header & scatter file.
  bindergen     : Generate binder related information

  sign-image    : Sign all the image generated.
  encrypt-image : Encrypt all the image generated.
  update-api    : Android default build action
                  (be executed if system setting or anything removed from API).
  check-modem   : Check modem image consistency.
  upadte-modem  : Update modem image located in system.img.
  modem-info    : Show modem version
  gen-relkey    : Generate releasekey for application signing.
  check-appres  : Check unused application resource.

  sdk           : Build sdk package.
  win_sdk       : Build sdk package with a few Windows tools.
  banyan_addon  : Build MTK sdk addon.
  banyan_addon_x86 :Build MTK sdk x86 addon.
  cts           : Build cts package.
  bootimage     : Build boot image(boot.img).
  cacheimage    : Build cache image(cache.img).
  systemimage   : Build system image(system.img).
  snod          : Build system image without dependency.
                  (that is, ONLY pack the system image, NOT checking its dependencies.)
  recoveryimage : Build recovery image(recovery.img).
  secroimage    : Build secro image(secro.img).
  factoryimage  : Build factory image(factory.img).
  userdataimage : Build userdata image(userdata.img).
  userdataimage-nodeps
                : Build userdata image without dependency.
                  (that is, ONLY pack the userdata image, NOT checking its dependencies.)
  dump-products : Dump products related configuration(PRODUCT_PACKAGE,PRODUCT_NAME ect.)
  target-files-package
                : Build the target files package.
                  (A zip of the directories that map to the target filesystem.
                   This zip can be used to create an OTA package or filesystem image
                   as a post-build step.)
  updatepackage : Build the update package.
  dist          : Build distribution package.

Modules:
  pl, preloader : Specify to build preloader.
  lk            : Specify to build little kernel.
  md32          : Specify to build DSP md32.
  tz, trustzone : Specify to build trusted execution environment.
  k,  kernel    : Specify to build kernel.
  dr, android   : Specify to build android.
  NULL          : Specify to build all components/modules in default.
  k <module path>
                : Specify to build kernel component/module with the source path.
  dr <module name>
                : Specify to build android component/module with module name.

Other tools:
  prebuilts/misc/linux-x86/ccache/ccache -M 10G
                : Set CCACHE pool size to 10GB

Example:
  ./mk -t e1k emigen
                : Generate EMI setting source code.
  ./mk -o=TARGET_BUILD_VARIANT=user e1k n
                : Start a user mode full build.
  ./mk listp    : List all available projects.
  ./mk e1k bootimage
                : Build bootimage for e1k project.
  ./mk e1k bm_new k
                : Build kernel for e1k project.
  ./makeMtk e1k c,bm_remake pl k
                : Clean & Build preloader and kernel for e1k project. 
  ./makeMtk e1k n k kernel/xxx/xxx
                : Build(full build) kernel component/module 
                  under the path "kernel/xxx/xxx" for e1k project.
  ./makeMtk e1k r dr Gallery
                : Rebuild android module named Gallery for e1k project.
  ./makeMtk e1k mm packages/apps/Settings
    : Change Directory to packages/apps/Settings and execute "mm"
  ./makeMtk e1k mma packages/apps/Settings
    : Change Directory to packages/apps/Settings and execute "mma"


__END_OF_USAGE

  exit 1;
}

sub writeINI
{
  @iniFields = qw(project build_mode);
  open (FILE_HANDLE, ">$ini") or die "cannot open $ini\n";
  foreach $m (@iniFields)
  {
    foreach $i (@mOpts)
    {
      @temp = split("=","$i");
      if ($temp[0] eq "TARGET_BUILD_VARIANT")
      { $build_mode = $temp[1];} 
    }
    if ($m eq "build_mode" && $build_mode eq "")
    {
      print FILE_HANDLE "$m = eng\n";
    }
    elsif ($m eq "project" && $flavor ne "")
    {
      print FILE_HANDLE "$m = $project\[$flavor\]\n";
    }
    else
    {
      $value = $${m};
      print FILE_HANDLE "$m = $value\n";
    }
  }
  close FILE_HANDLE;
}

sub p_system
{
  my ($cmd) = @_;
  my ($debugp) = 0;
  my $result;
  ($debugp != 0) && print("$cmd\n");
  ($performanceChk == 1) && print &CurrTimeStr . " system $cmd\n";
  $result = system("$cmd");
  ($performanceChk == 1) && print &CurrTimeStr . " exit $cmd\n";
  return $result;
}

sub chkMustEnv
{
  my $result;
  print "\n**********checking Env************\n";
  &p_system("chmod 755 mediatek/build/tools/checkEnv.py");
  $result = &p_system("python mediatek/build/tools/checkEnv.py -a >checkenv.log 2>&1");
  if ($result == 0)
  {
    print " Your building environment is OK\n"
          . " The detail information is in \"checkenv.log\"!\n"
          . "*************************************************\n";
  }
  else
  {
    print "Please check your building environment First!\n\n";
    if ($tee eq "TRUE")
    {
      system("cat checkenv.log");
    }
    else
    {
      open FILE, "<checkenv.log";
      while(<FILE>)
      {
        print if(/FAIL|WARNING/);
      }
      close(FILE);
    }
    die "\n The detail information is in \"checkenv.log\"!\n"
        . "***********************************************\n";
  }
}
sub chkDep
{
  my $module = $_[0];
  my $string = $module;
  if (!$module)
  {
    $string = all;
  }
#  print "*********************************\n \"$string\" Dependency Checking...\n";
  my $result;
  $result = &p_system("CUR_MODULE=$module make -f mediatek/build/makemtk.mk check-dep $cmdArg @mOpts > $logdir/${project}_check-dep.log 2>&1");
  &p_system("cat $logdir/${project}_check-dep.log");
  if ($result)
  {
    &p_system("cp -f $logdir/${project}_check-dep.log $logdir/${project}_check-dep.log_err");
    die "Please check all dependency error First !!";
  }
}

sub setBuildEnvVars
{
  my $bldProfile = shift;
  die "\"$bldProfile\" does NOT exist!\n" if (!-e $bldProfile);
  # Todo: error handling for '. $bldProfile' command
  my $envVarList = `. $bldProfile && env | grep ".*=.*"`;
  map
    {
      chomp;
      $ENV{$1}=$2 if (/(.*)=(.*)/);
    } split(/\n/, $envVarList);

  if ($DEBUG)
  {
    print "[START LOGGING]: Current build environment variables setting...\n";
    print "PATH=$ENV{PATH}\n";
    print "ANDROID_JAVA_HOME=$ENV{ANDROID_JAVA_HOME}\n";
    print "JAVA_HOME=$ENV{JAVA_HOME}\n";
    print "PYTHONPATH=$ENV{PYTHONPATH}\n";
    print "[END LOGGING]\n";
  }
}

sub setCacheEnvVars
{
	my $host_server;
	if ($ENV{"LSB_SUB_HOST"} ne "")
	{
		$host_server = $ENV{"LSB_SUB_HOST"};
	}
	else
	{
		$host_server = hostname;
	}
	print "Server = " . $host_server . "\n" if ($DEBUG);
	if (($host_server =~ /m\w\wswgbm/i) || ($ENV{"USER"} =~ /wcxbm/i) || ($ENV{"USER"} =~ /arsbm/i))
	{
		# ignore for special server/user
	}
	else
	{
		my $ccache_bin = "prebuilts/misc/linux-x86/ccache/ccache";
		if ((exists $ENV{"USE_CCACHE"}) && ($ENV{"USE_CCACHE"} eq ""))
		{
			# print "CCACHE: ignore for special setting\n";
		}
		elsif (-d $ENV{"HOME"} . "/.ccache")
		{
			my $size_cur = 0;
			my $size_max = 0;
			my @message = `$ccache_bin -s`;
			foreach my $line (@message)
			{
				if ($line =~ /^cache size\s+(\S+)\s+([GMK])bytes/)
				{
					$size_cur = $1;
					$size_cur /= 1024 if ($2 eq "M");
					$size_cur /= 1048576 if ($2 eq "K");
				}
				elsif ($line =~ /^max cache size\s+(\S+)\s+([GMK])bytes/)
				{
					$size_max = $1;
					$size_max /= 1024 if ($2 eq "M");
					$size_max /= 1048576 if ($2 eq "K");
				}
			}
			if ($size_cur + 0.9 > $size_max)
			{
				print "Warning: CCACHE is disabled due to cache pool is almost full\n";
				print "Current = $size_cur GB, max = $size_max GB\n";
				print "Please run \"$ccache_bin -C\" to clean cache pool first\n";
			}
			{
				# default cache is turned on
				$ENV{"USE_CCACHE"} = 1;
			}
		}
		else
		{
			print "Warning: CCACHE is disabled due to it has not been initialized before\n";
			print "Please run \"$ccache_bin -M 10G\" to set CCACHE pool size first\n";
		}
		if ((exists $ENV{"USE_DXCACHE"}) && ($ENV{"USE_DXCACHE"} eq ""))
		{
		}
		elsif (-d $ENV{"HOME"} . "/.dxcache")
		{
			if ($ENV{"USE_CCACHE"} == 1)
			{
				# dxcache follows ccache's rule
				$ENV{"USE_DXCACHE"} = 1;
			}
		}
	}
}

# interactive mode for inputting signature subject
sub inputSignSubject
{
  my $signature =
     {
       '0-C'            => ['C', '', 'Country Name (2 letter code)'],
       '1-ST'           => ['ST', '', 'State or Province Name (full name)'],
       '2-L'            => ['L', '', 'Locality Name (eg, city)'],
       '3-O'            => ['O', '', 'Organization Name (eg, company)'],
       '4-OU'           => ['OU', '', 'Organizational Unit Name (eg, section)'],
       '5-CN'           => ['CN', '', 'Common Name (eg, your name or your server hostname)'],
       '6-emailAddress' => ['emailAddress', '', 'Contact email address']
     };

  my $subjectStr = "";
  print "Please enter the signature subject as follows.\n";

  foreach my $k (sort keys %$signature)
  {
    while (1)
    {
      if (!$signature->{$k}[1])
      {
        # print promote message
        print "$signature->{$k}[2]: ";
        $signature->{$k}[1] = <STDIN>;
        chomp $signature->{$k}[1];
        if ($signature->{$k}[1])
        {
          $subjectStr .= "/$signature->{$k}[0]=$signature->{$k}[1]";
          last;
        }
      }
    }
  }

  return $subjectStr;
}

