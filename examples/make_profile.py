#!/usr/bin/env python3
"""Create a private matched HAPS gNB/UE/channel triplet for OAI 9313cbe.

Only writes a new output directory; does not modify templates, OAI or the core.
--profile sa retains broadcast SIB1/SIB19. --profile e9 is coreless --do-ra.
"""
import argparse
import json
from pathlib import Path
import re

EXAMPLES = Path(__file__).resolve().parent
OAI_COMMIT = '9313cbea82bc3c6b8ff695dd808a0cde5f431724'
LONGITUDES = {90:32.859700000,60:32.994297811,30:33.261831217,10:34.122126213}


def masked(text):
    pattern = r'"(?:\\.|[^"\\])*"|/\*[\s\S]*?\*/|//[^\n]*|\#[^\n]*'
    return re.sub(pattern, lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0]), text)


def set_values(text, values):
    for name,value in values.items():
        matches=list(re.finditer(r'\b'+re.escape(name)+r'\s*=\s*[^;]*;',masked(text)))
        if not matches: raise ValueError('Missing assignment: '+name)
        for m in reversed(matches):
            text=text[:m.start()]+name+' = '+str(value)+';'+text[m.end():]
    return text


def include(text):
    text,n=re.subn(r'@include\s+"[^"\n]+"','@include "chan.conf"',text)
    if n!=1: raise ValueError('Expected exactly one channel include')
    return text


def prepare(profile, angle, scenario='rural', seed=1, server_ip='127.0.0.1', port=4043):
    gnb=(EXAMPLES/('gnb.sa.band78.fr1.106PRB.haps.rfsim.e%d.conf'%angle)).read_text()
    ue=(EXAMPLES/('ue.haps.e%d.conf'%angle)).read_text()
    if profile=='e9':
        channel=(EXAMPLES/'channelmod_haps_3gpp_itu.e9-delay-clutter-on.conf').read_text()
        channel=set_values(channel,{'ue_longitude_deg':'%.9f'%LONGITUDES[angle],
            'scenario':json.dumps(scenario),'random_seed':seed})
        # OAI's --do-ra mode has no broadcast SIB1 on this pinned revision.
        # NTN configuration remains in the fresh gNB-generated reconfiguration.
        gnb=set_values(gnb,{'cu_sibs':'()','du_sibs':'[]'})
    else:
        channel=(EXAMPLES/('channelmod_haps_3gpp_itu.e%d.conf'%angle)).read_text()
    gnb=set_values(include(gnb),{'serverport':port})
    ue=set_values(include(ue),{'serveraddr':json.dumps(server_ip),'serverport':port})
    return {'gnb.conf':gnb,'ue.conf':ue,'chan.conf':channel}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile',choices=('sa','e9'),default='sa')
    p.add_argument('--elevation',type=int,choices=tuple(LONGITUDES),default=90)
    p.add_argument('--scenario',choices=('rural','dense_urban'),default='rural')
    p.add_argument('--seed',type=int,default=1)
    p.add_argument('--server-ip',default='127.0.0.1',help='RFsim host address reachable by UE, including inside netns')
    p.add_argument('--port',type=int,default=4043)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if not (1<=args.seed<=0xffffffff and 1024<=args.port<=65535):
        p.error('seed must be 1..2^32-1 and port 1024..65535')
    if args.profile=='sa' and (args.scenario!='rural' or args.seed!=1):
        p.error('--scenario/--seed are E9 controls; nominal SA is fixed forced LoS')
    files=prepare(args.profile,args.elevation,args.scenario,args.seed,args.server_ip,args.port)
    output=args.output.expanduser().resolve()
    output.mkdir(parents=True,exist_ok=False,mode=0o700)
    for name,text in files.items():
        path=output/name;path.write_text(text);path.chmod(0o600)
    metadata=dict(oai_commit=OAI_COMMIT,profile=args.profile,elevation_deg=args.elevation,
        ue_longitude_deg=LONGITUDES[args.elevation],seed=args.seed if args.profile=='e9' else None,
        scenario=args.scenario,core_required=args.profile=='sa',
        radio_mode='normal SA' if args.profile=='sa' else '--do-ra CFRA without core')
    (output/'profile.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print('Created '+str(output)+'; '+metadata['radio_mode'])


if __name__=='__main__': main()
