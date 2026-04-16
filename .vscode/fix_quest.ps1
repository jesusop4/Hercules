$enc = [System.Text.Encoding]::GetEncoding(1252)
$filePath = "C:\Users\nKorea\Desktop\Harus\Hercules\npc\ragnayokai\harus\gerais\quest.txt"

$f3 = [char]0xF3  # o (Latin-1)
$bb = [char]0xBB  # >> (Latin-1)
$e3 = [char]0xE3  # a~ (Latin-1)
$ea = [char]0xEA  # e^ (Latin-1)
$bul = [char]0x95 # bullet (Windows-1252)
$tab = [char]9

# Usar placeholders para chars especiais dentro do heredoc
# [BB]=», [E3]=ã, [EA]=ê, [F3]=ó, [95]=•
$c = @'
//===== eAthena Script =======================================
//= Euphy's Quest Shop
//===== By: ==================================================
//= Euphy
//===== Current Version: =====================================
//= 1.4a - eAthena
//===== Description: =========================================
//= A dynamic quest shop based on Lunar's, with easier config.
//= Includes support for multiple shops & cashpoints.
//= Item Preview script by ToastOfDoom.
//============================================================
					
prontera,143,179,6	script	Geraldo	448,{
function Add; function Chk; function Slot; function A_An;

	dispbottom "Selecione o item desejado e o compre para abrir os requerimentos.";
	set .@cat, select("^00CC00[[BB]]^000000 Quests Shield:^00CC00[[BB]]^000000 Quests bRO");
	if (.@cat == 1) {
		set .@i, select("^00CC00[[BB]]^000000  Topo:^00CC00[[BB]]^000000  Meio:^00CC00[[BB]]^000000  Baixo:^00CC00[[BB]]^000000  Acess[F3]rio");
	} else {
		set .@i, 5;
	}
	callshop "qshop"+.@i,1;
	npcshopattach "qshop"+.@i;
	end;


function Add {
	if (getitemname(getarg(1))=="null") {
		consolemes 0, "Quest reward #"+getarg(1)+" invalid (skipped)."; return; }
	for(set .@n,5; .@n<127; set .@n,.@n+2) {
		if (!getarg(.@n,0)) break;
		if (getitemname(getarg(.@n))=="null") {
			consolemes 0, "Quest requirement #"+getarg(.@n)+" invalid (skipped)."; return; } }
	for(set .@i,2; .@i<.@n; set .@i,.@i+1)
		set getd(".q_"+getarg(1)+"["+(.@i-2)+"]"), getarg(.@i);
	npcshopadditem "qshop"+getarg(0),getarg(1),((.ShowZeny)?getarg(3):0);
	sleep 1;
	return; }
function Chk {
	if (getarg(0)<getarg(1)) { set @qe0,1; return "^FF0000"; }
	else return "^00FF00"; }
function Slot {
	set .@s$,getitemname(getarg(0));
	switch(.ShowSlot){
		case 1: if (!getitemslots(getarg(0))) return .@s$;
		case 2: if (getiteminfo(getarg(0),11)>0) return .@s$+" ["+getitemslots(getarg(0))+"]";
		default: return .@s$; } }
function A_An {
	setarray .@A$[0],"a","e","i","o","u";
	set .@B$, "_"+getarg(0);
	for(set .@i,0; .@i<5; set .@i,.@i+1)
		if (compare(.@B$,"_"+.@A$[.@i])) return "an "+getarg(0);
	return "a "+getarg(0); }

OnBuyItem:
	set .@q[0],@bought_nameid;
	copyarray .@q[1],getd(".q_"+@bought_nameid+"[0]"),getarraysize(getd(".q_"+@bought_nameid+"[0]"));
	if (!.@q[1]) { message strcharinfo(0),"An error has occurred."; end; }
	mes "[Geraldo]";
	mes "Recompensa: ^0055FF"+((.@q[1]>1)?.@q[1]+"x ":"")+Slot(.@q[0])+"^000000";
	mes "Requerimentos:";
	if (.@q[2]) mes " ^777777[95]^000000 "+Chk(Zeny,.@q[2])+.@q[2]+" Zeny^000000";
	if (.@q[3]) mes " ^777777[95]^000000 "+Chk(getd(.Points$[0]),.@q[3])+.@q[3]+" "+.Points$[1]+" ("+getd(.Points$[0])+"/"+.@q[3]+")^000000";
	if (.@q[4]) for(set .@i,4; .@i<getarraysize(.@q); set .@i,.@i+2)
		mes " ^777777[95]^000000 "+Chk(countitem(.@q[.@i]),.@q[.@i+1])+((.DisplayID)?"{"+.@q[.@i]+"} ":"")+Slot(.@q[.@i])+" ("+countitem(.@q[.@i])+"/"+.@q[.@i+1]+")^000000";
	next;
	set @qe1, getiteminfo(.@q[0],5); set @qe2, getiteminfo(.@q[0],11);
	addtimer 1000, strnpcinfo(1)+"::OnEnd";
	while(1){
		switch(select("^00CC00[[BB]]^000000 Criar ^0055FF"+getitemname(.@q[0])+"^000000:"+((((@qe1&1) || (@qe1&256) || (@qe1&512)) && @qe2>0 && !@qe6)?" ~ Experimentar...":"")+":^777777[X] Cancelar^000000")) {
			case 1:
				if (@qe0) { 
					mes "[Geraldo]";
					mes "Est[E3]o faltando um ou mais itens para completar a quest.";
					close; }
				if (!checkweight(.@q[0],.@q[1])) {
					mes "[Geraldo]";
					mes "^FF0000Voc[EA] precisa de "+(((.@q[1]*getiteminfo(.@q[0],6))+Weight-MaxWeight)/10)+" a mais na capacidade de carga para completar a quest.^000000";
					close; }
				if (.@q[2]) set Zeny, Zeny-.@q[2];
				if (.@q[3]) setd .Points$[0], getd(.Points$[0])-.@q[3];
				if (.@q[4]) for(set .@i,4; .@i<getarraysize(.@q); set .@i,.@i+2)
					delitem .@q[.@i],.@q[.@i+1];
				getitem .@q[0],.@q[1];
				if (.Announce) announce "[Geraldo]: "+strcharinfo(0)+" acaba de completar a Quest ["+getitemname(.@q[0])+"].",0;
				specialeffect 699;
				close;
			case 2:
				set @qe3, getlook(3); set @qe4, getlook(4); set @qe5, getlook(5);
				if (@qe1&1) atcommand "@changelook 3 "+@qe2;
				if (@qe1&256) atcommand "@changelook 1 "+@qe2;
				if (@qe1&512) atcommand "@changelook 2 "+@qe2;
				set @qe6,1;
				break;
			case 3:
				close; } }
OnEnd:
	if (@qe6) { atcommand "@changelook 3 "+@qe3; atcommand "@changelook 1 "+@qe4; atcommand "@changelook 2 "+@qe5; }
	for(set .@i,0; .@i<7; set .@i,.@i+1) setd "@qe"+.@i,0;
	end;
OnInit:
// --------------------- Config ---------------------
// Custom points, if needed: "<variable>","<name to display>"
	setarray .Points$[0],"#CASHPOINTS","Cash Points";

	set .Announce,1;	// Announce quest completion? (1: yes / 0: no)
	set .ShowSlot,1;	// Show item slots? (2: all equipment / 1: if slots > 0 / 0: never)
	set .DisplayID,1;	// Show item IDs? (1: yes / 0: no)
	set .ShowZeny,0;	// Show Zeny cost, if any? (1: yes / 0: no)

// Shop 1=Yokai Topo, 2=Yokai Meio, 3=Yokai Baixo, 4=Yokai Acess[F3]rio, 5=Quests bRO
// Add(<shop number>,<reward ID>,<reward amount>,<Zeny cost>,<point cost>,
//     <required item ID>,<required item amount>{,...});
// Shop number corresponds with order above (default is 1).
// Note: Do NOT use a reward item more than once!
	Add(1,5170,1,0,0,5172,1,7063,100,982,1);
	Add(1,5340,1,0,0,5170,1,4253,5,4411,5,7047,200);
	Add(1,5341,1,0,0,5170,1,4253,5,4412,5,7047,200);
	Add(1,5342,1,0,0,5170,1,4253,5,4064,5,7047,200);
	Add(1,5343,1,0,0,5170,1,4253,5,4129,5,7047,200);
	Add(1,5344,1,0,0,5170,1,4253,5,4078,5,7047,200);
	Add(1,5345,1,0,0,5170,1,4253,5,4293,5,7047,200);
	Add(1,5361,1,0,0,5096,1,5005,5,7568,150,7216,350);
	Add(1,5481,1,0,0,4368,3,979,5,2221,5,945,500);
	Add(1,5563,1,0,0,4411,2,4412,2,4064,2,4129,2,4078,2,4293,2,7436,10,7054,500);
	Add(1,5491,1,0,0,4109,5,994,350,7097,350,7122,350,1041,350);
	Add(1,5463,1,0,0,5096,1,975,5,976,5,978,5,979,5,980,5,981,5,982,5,983,5);

	Add(5,5086,1,0,0,1095,3000,2288,1);
	Add(5,2284,1,0,0,923,20);
	Add(5,5110,1,0,0,526,2,7270,1,941,1,10004,1);
	Add(5,2296,1,0,0,2243,1,999,100);
	Add(5,5057,1,0,0,2213,1,983,1,914,200);
	Add(5,5016,1,0,0,1030,10);
	Add(5,2214,1,0,0,949,100,706,1,722,1,2213,1);
	Add(5,5107,1,0,0,519,50,7031,50,548,50,539,50);
	Add(5,5082,1,0,0,921,300);
	Add(5,2283,1,0,0,724,1,5001,1,949,200);
	Add(5,5069,1,0,0,1022,999);
	Add(5,5001,1,0,0,999,40,984,1,970,1,1003,1);
	Add(5,5070,1,0,0,7216,300,7097,300,2211,1,982,1);
	Add(5,5071,1,0,0,5010,1,5049,1,7101,10);
	Add(5,5073,1,0,0,2285,1,1550,1);
	Add(5,2278,1,0,0,705,10,909,10,914,10);
	Add(5,5117,1,0,0,731,10,748,2,982,1);
	Add(5,5094,1,0,0,909,10000,931,10000,968,100);
	Add(5,5004,1,0,0,701,5);
	Add(5,5012,1,0,0,710,1,703,1,704,1,708,1);
	Add(5,2293,1,0,0,1049,4);
	Add(5,5109,1,0,0,10015,1,10007,1,975,1,5032,1);
	Add(5,5083,1,0,0,2244,1,2209,1,10007,1);
	Add(5,5108,1,0,0,7301,1887,5120,1,611,10);
	Add(5,5078,1,0,0,5033,1,5064,1);
	Add(5,2272,1,0,0,1019,50,983,1);
	Add(5,5059,1,0,0,5030,1,7213,100,7217,100,7161,300);
	Add(5,5077,1,0,0,2278,1,975,1);
	Add(5,2292,1,0,0,999,50);
	Add(5,5115,1,0,0,983,1,7267,999,749,1);
	Add(5,5121,1,0,0,7315,369,7263,1,660,1,7099,30);
	Add(5,5074,1,0,0,2286,1,2254,1);
	Add(5,5068,1,0,0,2286,1,2255,1);
	Add(5,5025,1,0,0,2229,1,2254,1,7036,5);
	Add(5,5038,1,0,0,1038,600,7048,40);
	Add(5,5091,1,0,0,10016,1,714,1,969,3);
	Add(5,5080,1,0,0,10006,1,714,1,969,3);
	Add(5,5081,1,0,0,2249,1,714,1,969,3);
	Add(5,5079,1,0,0,2294,1,7220,400);
	Add(5,5063,1,0,0,970,1,930,500);
	Add(5,5061,1,0,0,2269,1,999,10);
	Add(5,5042,1,0,0,10007,1,968,50);
	Add(5,5048,1,0,0,5041,1,999,10);
	Add(5,5047,1,0,0,2271,1,975,1);
	Add(5,5041,1,0,0,7013,1200);
	Add(5,5040,1,0,0,7047,100);
	Add(5,5024,1,0,0,529,10,530,10,539,20,999,10,538,15);
	Add(5,5028,1,0,0,2279,1,7035,50,526,100);
	Add(5,5026,1,0,0,1036,450,949,330,539,120,982,1);
	Add(5,5036,1,0,0,2608,1,7069,500);
	Add(5,5034,1,0,0,2233,1,746,20);
	Add(5,5049,1,0,0,1099,1500);
	Add(5,5052,1,0,0,2211,1,978,1,7003,300);
	Add(5,2273,1,0,0,2275,1,998,50,733,1);
	Add(5,5018,1,0,0,2247,1,916,300);
	Add(5,2281,1,0,0,998,20,707,1);
	Add(5,2280,1,0,0,1019,120);
	Add(5,5058,1,0,0,2233,1,983,1,7206,300,7030,1);
	Add(5,5064,1,0,0,945,600,7030,1);
	Add(5,5084,1,0,0,1026,1000,7065,100,945,10,7030,1);
	Add(5,5065,1,0,0,624,1,959,300,551,50,1023,1,938,100,7030,1);
	Add(5,5032,1,0,0,1059,250,2221,1,2227,1,7063,600);
	Add(5,5027,1,0,0,2252,1,1036,400,7001,50,4052,1);
	Add(5,5045,1,0,0,2252,1,1054,450,943,1200 );
	Add(5,5031,1,0,0,5009,1,5028,1,747,1,999,25);
	Add(5,5023,1,0,0,1059,150,907,100,978,1);
	Add(5,5021,1,0,0,2233,1,969,1,999,20,949,80,938,800);
	Add(5,5043,1,0,0,2281,1,1048,50);
	Add(5,5033,1,0,0,1036,20,2213,1,7065,300,7012,200);
	Add(5,5039,1,0,0,7030,50,978,1,5015,1);
	Add(5,5029,1,0,0,7068,300,7033,850,1015,1);
	Add(5,5050,1,0,0,5037,1,7064,500);
	Add(5,5060,1,0,0,2236,1,7151,100,7111,100);
	Add(5,5062,1,0,0,2280,1,7197,300,7150,300);
	Add(5,5075,1,0,0,2248,1,7030,108,7194,108,7120,4);
	Add(5,5067,1,0,0,5062,1,952,50,1907,1);
	Add(5,5076,1,0,0,2226,1,7038,500);


// --------------------------------------------------
	for(set .@i,1; .@i<=5; set .@i,.@i+1)
		npcshopdelitem "qshop"+.@i,909;
	end;
}
'@

# Substituir placeholders pelos chars Latin-1 reais
$c = $c.Replace("[BB]", $bb)
$c = $c.Replace("[E3]", $e3)
$c = $c.Replace("[EA]", $ea)
$c = $c.Replace("[F3]", $f3)
$c = $c.Replace("[95]", $bul)

# Adicionar dummy shops com tabs reais
$c += "`r`n"
$c += "// -------- Dummy data (1=Topo, 2=Meio, 3=Baixo, 4=Acess" + $f3 + "rio, 5=bRO) --------`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop1" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop2" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop3" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop4" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop5" + $tab + "-1,909:-1`r`n"

# Salvar como Latin-1
[System.IO.File]::WriteAllBytes($filePath, $enc.GetBytes($c))

# Verificar resultado
$bytes = [System.IO.File]::ReadAllBytes($filePath)
Write-Host "Bytes: $($bytes.Length)"
$efbfbd = 0
for ($i = 0; $i -lt ($bytes.Length - 2); $i++) {
    if ($bytes[$i] -eq 0xEF -and $bytes[$i+1] -eq 0xBF -and $bytes[$i+2] -eq 0xBD) { $efbfbd++ }
}
Write-Host "Replacement chars (EF BF BD): $efbfbd"
$textCheck = [System.IO.File]::ReadAllText($filePath, $enc)
$v1 = if ($textCheck.Contains("Quests Shield")) { "OK" } else { "FALHOU" }
$v2 = if ($textCheck.Contains("00CC00")) { "OK" } else { "FALHOU" }
$v3 = if ($textCheck.Contains("Add(5,")) { "OK" } else { "FALHOU" }
$v4 = if ($textCheck.Contains("qshop5")) { "OK" } else { "FALHOU" }
Write-Host "Menu Shield: $v1"
Write-Host "Cores: $v2"
Write-Host "Add(5,...): $v3"
Write-Host "qshop5: $v4"

$c  = "//===== eAthena Script =======================================`r`n"
$c += "//= Euphy's Quest Shop`r`n"
$c += "//===== By: ==================================================`r`n"
$c += "//= Euphy`r`n"
$c += "//===== Current Version: =====================================`r`n"
$c += "//= 1.4a - eAthena`r`n"
$c += "//===== Description: =========================================`r`n"
$c += "//= A dynamic quest shop based on Lunar's, with easier config.`r`n"
$c += "//= Includes support for multiple shops & cashpoints.`r`n"
$c += "//= Item Preview script by ToastOfDoom.`r`n"
$c += "//============================================================`r`n"
$c += "`t`t`t`t`t`r`n"
$c += "prontera,143,179,6`tscript`tGeraldo`t448,{`r`n"
$c += "function Add; function Chk; function Slot; function A_An;`r`n"
$c += "`r`n"
$c += "`tdispbottom `"Selecione o item desejado e o compre para abrir os requerimentos.`";`r`n"
$c += "`tset .@cat, select(`"^00CC00[" + $bb + "]^000000 Quests Shield:^00CC00[" + $bb + "]^000000 Quests bRO`");`r`n"
$c += "`tif (.@cat == 1) {`r`n"
$c += "`t`tset .@i, select(`"^00CC00[" + $bb + "]^000000  Topo:^00CC00[" + $bb + "]^000000  Meio:^00CC00[" + $bb + "]^000000  Baixo:^00CC00[" + $bb + "]^000000  Acess" + $f3 + "rio`");`r`n"
$c += "`t} else {`r`n"
$c += "`t`tset .@i, 5;`r`n"
$c += "`t}`r`n"
$c += "`tcallshop `"qshop`"+.@i,1;`r`n"
$c += "`tnpcshopattach `"qshop`"+.@i;`r`n"
$c += "`tend;`r`n"
$c += "`r`n"
$c += "`r`n"
$c += "function Add {`r`n"
$c += "`tif (getitemname(getarg(1))==`"null`") {`r`n"
$c += "`t`tconsolemes 0, `"Quest reward #`"+getarg(1)+`" invalid (skipped).`"; return; }`r`n"
$c += "`tfor(set .@n,5; .@n<127; set .@n,.@n+2) {`r`n"
$c += "`t`tif (!getarg(.@n,0)) break;`r`n"
$c += "`t`tif (getitemname(getarg(.@n))==`"null`") {`r`n"
$c += "`t`t`tconsolemes 0, `"Quest requirement #`"+getarg(.@n)+`" invalid (skipped).`"; return; } }`r`n"
$c += "`tfor(set .@i,2; .@i<.@n; set .@i,.@i+1)`r`n"
$c += "`t`tset getd(`".q_`"+getarg(1)+`"[`"+(.@i-2)+`"]`"), getarg(.@i);`r`n"
$c += "`tnpcshopadditem `"qshop`"+getarg(0),getarg(1),((.ShowZeny)?getarg(3):0);`r`n"
$c += "`tsleep 1;`r`n"
$c += "`treturn; }`r`n"
$c += "function Chk {`r`n"
$c += "`tif (getarg(0)<getarg(1)) { set @qe0,1; return `"^FF0000`"; }`r`n"
$c += "`telse return `"^00FF00`"; }`r`n"
$c += "function Slot {`r`n"
$c += "`tset .@s`$,getitemname(getarg(0));`r`n"
$c += "`tswitch(.ShowSlot){`r`n"
$c += "`t`tcase 1: if (!getitemslots(getarg(0))) return .@s`$;`r`n"
$c += "`t`tcase 2: if (getiteminfo(getarg(0),11)>0) return .@s`$+`" [`"+getitemslots(getarg(0))+`"]`";`r`n"
$c += "`t`tdefault: return .@s`$; } }`r`n"
$c += "function A_An {`r`n"
$c += "`tsetarray .@A`$[0],`"a`",`"e`",`"i`",`"o`",`"u`";`r`n"
$c += "`tset .@B`$, `"_`"+getarg(0);`r`n"
$c += "`tfor(set .@i,0; .@i<5; set .@i,.@i+1)`r`n"
$c += "`t`tif (compare(.@B`$,`"_`"+.@A`$[.@i])) return `"an `"+getarg(0);`r`n"
$c += "`treturn `"a `"+getarg(0); }`r`n"
$c += "`r`n"
$c += "OnBuyItem:`r`n"
$c += "`tset .@q[0],@bought_nameid;`r`n"
$c += "`tcopyarray .@q[1],getd(`".q_`"+@bought_nameid+`"[0]`"),getarraysize(getd(`".q_`"+@bought_nameid+`"[0]`"));`r`n"
$c += "`tif (!.@q[1]) { message strcharinfo(0),`"An error has occurred.`"; end; }`r`n"
$c += "`tmes `"[Geraldo]`";`r`n"
$c += "`tmes `"Recompensa: ^0055FF`"+((.@q[1]>1)?.@q[1]+`"x `":`"``)+Slot(.@q[0])+`"^000000`";`r`n"
$c += "`tmes `"Requerimentos:`";`r`n"
# bullet = 0x95 (•) em Windows-1252
$c += "`tif (.@q[2]) mes `" ^777777" + [char]0x95 + "^000000 `"+Chk(Zeny,.@q[2])+.@q[2]+`" Zeny^000000`";`r`n"
$c += "`tif (.@q[3]) mes `" ^777777" + [char]0x95 + "^000000 `"+Chk(getd(.Points`$[0]),.@q[3])+.@q[3]+`" `"+.Points`$[1]+`" (`"+getd(.Points`$[0])+`"/`"+.@q[3]+`")^000000`";`r`n"
$c += "`tif (.@q[4]) for(set .@i,4; .@i<getarraysize(.@q); set .@i,.@i+2)`r`n"
$c += "`t`tmes `" ^777777" + [char]0x95 + "^000000 `"+Chk(countitem(.@q[.@i]),.@q[.@i+1])+((.DisplayID)?`"{`"+.@q[.@i]+`"} `":`"``)+Slot(.@q[.@i])+`" (`"+countitem(.@q[.@i])+`"/`"+.@q[.@i+1]+`")^000000`";`r`n"
$c += "`tnext;`r`n"
$c += "`tset @qe1, getiteminfo(.@q[0],5); set @qe2, getiteminfo(.@q[0],11);`r`n"
$c += "`taddtimer 1000, strnpcinfo(1)+`"::OnEnd`";`r`n"
$c += "`twhile(1){`r`n"
$switchLine  = "`t`tswitch(select(`"^00CC00[" + $bb + "]^000000 Criar ^0055FF`"+getitemname(.@q[0])+`"^000000:`"+""
"""
$switchLine += """((((@qe1&1) || (@qe1&256) || (@qe1&512)) && @qe2>0 && !@qe6)?`" ~ Experimentar...`":`"`")+`":^777777[X] Cancelar^000000`")) {`r`n"
$c += $switchLine
$c += "`t`t`tcase 1:`r`n"
$c += "`t`t`t`tif (@qe0) { `r`n"
$c += "`t`t`t`t`tmes `"[Geraldo]`";`r`n"
$c += "`t`t`t`t`tmes `"Est" + $e3 + "o faltando um ou mais itens para completar a quest.`";`r`n"
$c += "`t`t`t`t`tclose; }`r`n"
$c += "`t`t`t`tif (!checkweight(.@q[0],.@q[1])) {`r`n"
$c += "`t`t`t`t`tmes `"[Geraldo]`";`r`n"
$c += "`t`t`t`t`tmes `"^FF0000Voc" + $ea + " precisa de `"+(((.@q[1]*getiteminfo(.@q[0],6))+Weight-MaxWeight)/10)+"`" a mais na capacidade de carga para completar a quest.^000000`";`r`n"
$c += "`t`t`t`t`tclose; }`r`n"
$c += "`t`t`t`tif (.@q[2]) set Zeny, Zeny-.@q[2];`r`n"
$c += "`t`t`t`tif (.@q[3]) setd .Points`$[0], getd(.Points`$[0])-.@q[3];`r`n"
$c += "`t`t`t`tif (.@q[4]) for(set .@i,4; .@i<getarraysize(.@q); set .@i,.@i+2)`r`n"
$c += "`t`t`t`t`tdelitem .@q[.@i],.@q[.@i+1];`r`n"
$c += "`t`t`t`tgetitem .@q[0],.@q[1];`r`n"
$c += "`t`t`t`tif (.Announce) announce `"[Geraldo]: `"+strcharinfo(0)+`" acaba de completar a Quest [`"+getitemname(.@q[0])+`"].`",0;`r`n"
$c += "`t`t`t`tspecialeffect 699;`r`n"
$c += "`t`t`t`tclose;`r`n"
$c += "`t`t`tcase 2:`r`n"
$c += "`t`t`t`tset @qe3, getlook(3); set @qe4, getlook(4); set @qe5, getlook(5);`r`n"
$c += "`t`t`t`tif (@qe1&1) atcommand `"@changelook 3 `"+@qe2;`r`n"
$c += "`t`t`t`tif (@qe1&256) atcommand `"@changelook 1 `"+@qe2;`r`n"
$c += "`t`t`t`tif (@qe1&512) atcommand `"@changelook 2 `"+@qe2;`r`n"
$c += "`t`t`t`tset @qe6,1;`r`n"
$c += "`t`t`t`tbreak;`r`n"
$c += "`t`t`tcase 3:`r`n"
$c += "`t`t`t`tclose; } }`r`n"
$c += "OnEnd:`r`n"
$c += "`tif (@qe6) { atcommand `"@changelook 3 `"+@qe3; atcommand `"@changelook 1 `"+@qe4; atcommand `"@changelook 2 `"+@qe5; }`r`n"
$c += "`tfor(set .@i,0; .@i<7; set .@i,.@i+1) setd `"@qe`"+.@i,0;`r`n"
$c += "`tend;`r`n"
$c += "OnInit:`r`n"
$c += "// --------------------- Config ---------------------`r`n"
$c += "// Custom points, if needed: `"<variable>`",`"<name to display>`"`r`n"
$c += "`tsetarray .Points`$[0],`"#CASHPOINTS`",`"Cash Points`";`r`n"
$c += "`r`n"
$c += "`tset .Announce,1;`t// Announce quest completion? (1: yes / 0: no)`r`n"
$c += "`tset .ShowSlot,1;`t// Show item slots? (2: all equipment / 1: if slots > 0 / 0: never)`r`n"
$c += "`tset .DisplayID,1;`t// Show item IDs? (1: yes / 0: no)`r`n"
$c += "`tset .ShowZeny,0;`t// Show Zeny cost, if any? (1: yes / 0: no)`r`n"
$c += "`r`n"
$c += "// Shop 1=Yokai Topo, 2=Yokai Meio, 3=Yokai Baixo, 4=Yokai Acess" + $f3 + "rio, 5=Quests bRO`r`n"
$c += "// Add(<shop number>,<reward ID>,<reward amount>,<Zeny cost>,<point cost>,`r`n"
$c += "//     <required item ID>,<required item amount>{,...});`r`n"
$c += "// Shop number corresponds with order above (default is 1).`r`n"
$c += "// Note: Do NOT use a reward item more than once!`r`n"
$c += "`tAdd(1,5170,1,0,0,5172,1,7063,100,982,1);`r`n"
$c += "`tAdd(1,5340,1,0,0,5170,1,4253,5,4411,5,7047,200);`r`n"
$c += "`tAdd(1,5341,1,0,0,5170,1,4253,5,4412,5,7047,200);`r`n"
$c += "`tAdd(1,5342,1,0,0,5170,1,4253,5,4064,5,7047,200);`r`n"
$c += "`tAdd(1,5343,1,0,0,5170,1,4253,5,4129,5,7047,200);`r`n"
$c += "`tAdd(1,5344,1,0,0,5170,1,4253,5,4078,5,7047,200);`r`n"
$c += "`tAdd(1,5345,1,0,0,5170,1,4253,5,4293,5,7047,200);`r`n"
$c += "`tAdd(1,5361,1,0,0,5096,1,5005,5,7568,150,7216,350);`r`n"
$c += "`tAdd(1,5481,1,0,0,4368,3,979,5,2221,5,945,500);`r`n"
$c += "`tAdd(1,5563,1,0,0,4411,2,4412,2,4064,2,4129,2,4078,2,4293,2,7436,10,7054,500);`r`n"
$c += "`tAdd(1,5491,1,0,0,4109,5,994,350,7097,350,7122,350,1041,350);`r`n"
$c += "`tAdd(1,5463,1,0,0,5096,1,975,5,976,5,978,5,979,5,980,5,981,5,982,5,983,5);`r`n"
$c += "`r`n"
$c += "`tAdd(5,5086,1,0,0,1095,3000,2288,1);`r`n"
$c += "`tAdd(5,2284,1,0,0,923,20);`r`n"
$c += "`tAdd(5,5110,1,0,0,526,2,7270,1,941,1,10004,1);`r`n"
$c += "`tAdd(5,2296,1,0,0,2243,1,999,100);`r`n"
$c += "`tAdd(5,5057,1,0,0,2213,1,983,1,914,200);`r`n"
$c += "`tAdd(5,5016,1,0,0,1030,10);`r`n"
$c += "`tAdd(5,2214,1,0,0,949,100,706,1,722,1,2213,1);`r`n"
$c += "`tAdd(5,5107,1,0,0,519,50,7031,50,548,50,539,50);`r`n"
$c += "`tAdd(5,5082,1,0,0,921,300);`r`n"
$c += "`tAdd(5,2283,1,0,0,724,1,5001,1,949,200);`r`n"
$c += "`tAdd(5,5069,1,0,0,1022,999);`r`n"
$c += "`tAdd(5,5001,1,0,0,999,40,984,1,970,1,1003,1);`r`n"
$c += "`tAdd(5,5070,1,0,0,7216,300,7097,300,2211,1,982,1);`r`n"
$c += "`tAdd(5,5071,1,0,0,5010,1,5049,1,7101,10);`r`n"
$c += "`tAdd(5,5073,1,0,0,2285,1,1550,1);`r`n"
$c += "`tAdd(5,2278,1,0,0,705,10,909,10,914,10);`r`n"
$c += "`tAdd(5,5117,1,0,0,731,10,748,2,982,1);`r`n"
$c += "`tAdd(5,5094,1,0,0,909,10000,931,10000,968,100);`r`n"
$c += "`tAdd(5,5004,1,0,0,701,5);`r`n"
$c += "`tAdd(5,5012,1,0,0,710,1,703,1,704,1,708,1);`r`n"
$c += "`tAdd(5,2293,1,0,0,1049,4);`r`n"
$c += "`tAdd(5,5109,1,0,0,10015,1,10007,1,975,1,5032,1);`r`n"
$c += "`tAdd(5,5083,1,0,0,2244,1,2209,1,10007,1);`r`n"
$c += "`tAdd(5,5108,1,0,0,7301,1887,5120,1,611,10);`r`n"
$c += "`tAdd(5,5078,1,0,0,5033,1,5064,1);`r`n"
$c += "`tAdd(5,2272,1,0,0,1019,50,983,1);`r`n"
$c += "`tAdd(5,5059,1,0,0,5030,1,7213,100,7217,100,7161,300);`r`n"
$c += "`tAdd(5,5077,1,0,0,2278,1,975,1);`r`n"
$c += "`tAdd(5,2292,1,0,0,999,50);`r`n"
$c += "`tAdd(5,5115,1,0,0,983,1,7267,999,749,1);`r`n"
$c += "`tAdd(5,5121,1,0,0,7315,369,7263,1,660,1,7099,30);`r`n"
$c += "`tAdd(5,5074,1,0,0,2286,1,2254,1);`r`n"
$c += "`tAdd(5,5068,1,0,0,2286,1,2255,1);`r`n"
$c += "`tAdd(5,5025,1,0,0,2229,1,2254,1,7036,5);`r`n"
$c += "`tAdd(5,5038,1,0,0,1038,600,7048,40);`r`n"
$c += "`tAdd(5,5091,1,0,0,10016,1,714,1,969,3);`r`n"
$c += "`tAdd(5,5080,1,0,0,10006,1,714,1,969,3);`r`n"
$c += "`tAdd(5,5081,1,0,0,2249,1,714,1,969,3);`r`n"
$c += "`tAdd(5,5079,1,0,0,2294,1,7220,400);`r`n"
$c += "`tAdd(5,5063,1,0,0,970,1,930,500);`r`n"
$c += "`tAdd(5,5061,1,0,0,2269,1,999,10);`r`n"
$c += "`tAdd(5,5042,1,0,0,10007,1,968,50);`r`n"
$c += "`tAdd(5,5048,1,0,0,5041,1,999,10);`r`n"
$c += "`tAdd(5,5047,1,0,0,2271,1,975,1);`r`n"
$c += "`tAdd(5,5041,1,0,0,7013,1200);`r`n"
$c += "`tAdd(5,5040,1,0,0,7047,100);`r`n"
$c += "`tAdd(5,5024,1,0,0,529,10,530,10,539,20,999,10,538,15);`r`n"
$c += "`tAdd(5,5028,1,0,0,2279,1,7035,50,526,100);`r`n"
$c += "`tAdd(5,5026,1,0,0,1036,450,949,330,539,120,982,1);`r`n"
$c += "`tAdd(5,5036,1,0,0,2608,1,7069,500);`r`n"
$c += "`tAdd(5,5034,1,0,0,2233,1,746,20);`r`n"
$c += "`tAdd(5,5049,1,0,0,1099,1500);`r`n"
$c += "`tAdd(5,5052,1,0,0,2211,1,978,1,7003,300);`r`n"
$c += "`tAdd(5,2273,1,0,0,2275,1,998,50,733,1);`r`n"
$c += "`tAdd(5,5018,1,0,0,2247,1,916,300);`r`n"
$c += "`tAdd(5,2281,1,0,0,998,20,707,1);`r`n"
$c += "`tAdd(5,2280,1,0,0,1019,120);`r`n"
$c += "`tAdd(5,5058,1,0,0,2233,1,983,1,7206,300,7030,1);`r`n"
$c += "`tAdd(5,5064,1,0,0,945,600,7030,1);`r`n"
$c += "`tAdd(5,5084,1,0,0,1026,1000,7065,100,945,10,7030,1);`r`n"
$c += "`tAdd(5,5065,1,0,0,624,1,959,300,551,50,1023,1,938,100,7030,1);`r`n"
$c += "`tAdd(5,5032,1,0,0,1059,250,2221,1,2227,1,7063,600);`r`n"
$c += "`tAdd(5,5027,1,0,0,2252,1,1036,400,7001,50,4052,1);`r`n"
$c += "`tAdd(5,5045,1,0,0,2252,1,1054,450,943,1200 );`r`n"
$c += "`tAdd(5,5031,1,0,0,5009,1,5028,1,747,1,999,25);`r`n"
$c += "`tAdd(5,5023,1,0,0,1059,150,907,100,978,1);`r`n"
$c += "`tAdd(5,5021,1,0,0,2233,1,969,1,999,20,949,80,938,800);`r`n"
$c += "`tAdd(5,5043,1,0,0,2281,1,1048,50);`r`n"
$c += "`tAdd(5,5033,1,0,0,1036,20,2213,1,7065,300,7012,200);`r`n"
$c += "`tAdd(5,5039,1,0,0,7030,50,978,1,5015,1);`r`n"
$c += "`tAdd(5,5029,1,0,0,7068,300,7033,850,1015,1);`r`n"
$c += "`tAdd(5,5050,1,0,0,5037,1,7064,500);`r`n"
$c += "`tAdd(5,5060,1,0,0,2236,1,7151,100,7111,100);`r`n"
$c += "`tAdd(5,5062,1,0,0,2280,1,7197,300,7150,300);`r`n"
$c += "`tAdd(5,5075,1,0,0,2248,1,7030,108,7194,108,7120,4);`r`n"
$c += "`tAdd(5,5067,1,0,0,5062,1,952,50,1907,1);`r`n"
$c += "`tAdd(5,5076,1,0,0,2226,1,7038,500);`r`n"
$c += "`r`n"
$c += "`r`n"
$c += "// --------------------------------------------------`r`n"
$c += "`tfor(set .@i,1; .@i<=5; set .@i,.@i+1)`r`n"
$c += "`t`tnpcshopdelitem `"qshop`"+.@i,909;`r`n"
$c += "`tend;`r`n"
$c += "}`r`n"
$c += "`r`n"
$c += "// -------- Dummy data (1=Topo, 2=Meio, 3=Baixo, 4=Acess" + $f3 + "rio, 5=bRO) --------`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop1" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop2" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop3" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop4" + $tab + "-1,909:-1`r`n"
$c += "-" + $tab + "shop" + $tab + "qshop5" + $tab + "-1,909:-1`r`n"

# Salvar como Latin-1
[System.IO.File]::WriteAllBytes($filePath, $enc.GetBytes($c))

# Verificar resultado
$bytes = [System.IO.File]::ReadAllBytes($filePath)
Write-Host "Bytes: $($bytes.Length)"
$efbfbd = 0
for ($i = 0; $i -lt ($bytes.Length - 2); $i++) {
    if ($bytes[$i] -eq 0xEF -and $bytes[$i+1] -eq 0xBF -and $bytes[$i+2] -eq 0xBD) { $efbfbd++ }
}
Write-Host "Replacement chars (EF BF BD): $efbfbd"
$textCheck = [System.IO.File]::ReadAllText($filePath, $enc)
$v1 = if ($textCheck.Contains("Quests Shield")) { "OK" } else { "FALHOU" }
$v2 = if ($textCheck.Contains("00CC00")) { "OK" } else { "FALHOU" }
$v3 = if ($textCheck.Contains("Add(5,")) { "OK" } else { "FALHOU" }
$v4 = if ($textCheck.Contains("qshop5")) { "OK" } else { "FALHOU" }
Write-Host "Menu Shield: $v1"
Write-Host "Cores: $v2"
Write-Host "Add(5,...): $v3"
Write-Host "qshop5: $v4"
Write-Host "Loop 1-5: $v5"
