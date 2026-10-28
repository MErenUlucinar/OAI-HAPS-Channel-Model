#!/usr/bin/env python3
"""Check release profile syntax, geometry and experiment isolation (no modems)."""
import importlib.util
import math
from pathlib import Path
import tempfile

try:
    import libconf
except ImportError:
    raise SystemExit('Install libconf==2.0.1 in a virtual environment; see examples/README.md')

ROOT=Path(__file__).resolve().parents[1]
EXAMPLES=ROOT/'examples'
spec=importlib.util.spec_from_file_location('haps_profiles',EXAMPLES/'make_profile.py')
profiles=importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiles)


def read(path):
    with path.open() as stream:
        return libconf.load(stream,filename=str(path),includedir=str(path.parent))


def ecef(lat,lon,height):
    lat,lon=math.radians(lat),math.radians(lon)
    a,f=6378137.0,1/298.257223563
    e2=f*(2-f);n=a/math.sqrt(1-e2*math.sin(lat)**2)
    return ((n+height)*math.cos(lat)*math.cos(lon),(n+height)*math.cos(lat)*math.sin(lon),
            (n*(1-e2)+height)*math.sin(lat))


def check_shared(config):
    if 'gNBs' in config:
        gnb=config['gNBs'][0];plmn=gnb['plmn_list'][0]
        assert (plmn['mcc'],plmn['mnc'],plmn['mnc_length'])==(208,99,2)
        assert tuple(plmn['snssaiList'])==({'sst':1},)
        assert gnb['do_SRS']=='none' and gnb['min_rxtxtime']==6
    if 'uicc0' in config:
        uicc=config['uicc0']
        assert str(uicc['imsi']).startswith('20899')
        assert not any(k in uicc for k in ('dnn','nssai_sst','nssai_sd'))
        assert tuple(uicc['pdu_sessions'])==({'dnn':'oai','nssai_sst':1},)
    if 'rfsimulator' in config:
        radio=config['rfsimulator']
        assert isinstance(radio,(list,tuple)) and len(radio)==1
        assert tuple(radio[0]['options'])==('chanmod',)
    if 'hapsmod' in config:
        models=config['hapsmod'];assert len(models)==2
        assert {m['model_name'] for m in models}=={'rfsimu_channel_enB0','rfsimu_channel_ue0'}
        for m in models:
            assert m['enable_gas']==int(m['model_name']=='rfsimu_channel_enB0')
            for name in ('enable_noise','enable_doppler','enable_fractional_delay','enable_fast_fading'):
                assert m[name]==0


def check_triplet(gnb,ue,channel,angle,mode,scenario='rural',seed=1):
    for value in (gnb,ue,channel):check_shared(value)
    g=gnb['gNBs'][0]
    assert tuple(g['cu_sibs'])==((2,) if mode=='sa' else ())
    assert tuple(g['du_sibs'])==((19,) if mode=='sa' else ())
    xyz=ecef(39.9334,profiles.LONGITUDES[angle],0)
    pos=ue['position0']
    assert max(abs(pos[k]-v) for k,v in zip('xyz',xyz))<0.000501
    haps=ecef(39.9334,32.8597,20000)
    common=g['servingCellConfigCommon'][0]
    for axis,v in zip('XYZ',haps):assert common['position%s-r17'%axis]==round(v/1.3)
    expected={90:4099,60:4730,30:8160,10:22525}
    samples=math.floor(math.dist(haps,xyz)/299792458*61440000+0.5)
    assert samples==expected[angle]
    for data in (gnb,ue,channel):
        for model in data['hapsmod']:
            assert model['ue_longitude_deg']==profiles.LONGITUDES[angle]
            assert model['enable_propagation_delay']==1
            assert model['enable_clutter']==int(mode=='e9')
            assert model['enable_shadow_fading']==int(mode=='e9')
            assert model['los_mode']==('probabilistic' if mode=='e9' else 'forced_los')
            assert model['link_budget_offset_db']==(112.08 if mode=='e9' else 129.64)
            if mode=='e9':assert model['scenario']==scenario and model['random_seed']==seed


def main():
    paths=sorted(EXAMPLES.glob('*.conf'))
    for path in paths:check_shared(read(path))
    for angle in profiles.LONGITUDES:
        check_triplet(read(EXAMPLES/('gnb.sa.band78.fr1.106PRB.haps.rfsim.e%d.conf'%angle)),
                      read(EXAMPLES/('ue.haps.e%d.conf'%angle)),
                      read(EXAMPLES/('channelmod_haps_3gpp_itu.e%d.conf'%angle)),angle,'sa')
    off_g=read(EXAMPLES/'gnb.sa.band78.fr1.106PRB.haps.rfsim.delay-off.conf')
    off_u=read(EXAMPLES/'ue.haps.delay-off.conf')
    assert all(m['enable_propagation_delay']==0 for c in (off_g,off_u) for m in c['hapsmod'])
    for scenario in ('rural','dense_urban'):
        for angle in profiles.LONGITUDES:
            for seed in (1,25,100):
                with tempfile.TemporaryDirectory() as name:
                    directory=Path(name)
                    for filename,text in profiles.prepare('e9',angle,scenario,seed,'10.231.0.1',4044).items():
                        (directory/filename).write_text(text)
                    g,u,c=[read(directory/f) for f in ('gnb.conf','ue.conf','chan.conf')]
                    check_triplet(g,u,c,angle,'e9',scenario,seed)
                    assert u['rfsimulator'][0]['serveraddr']=='10.231.0.1'
                    assert g['rfsimulator'][0]['serverport']==u['rfsimulator'][0]['serverport']==4044
    print('PASS: %d static configs, 4 matched SA triplets, delay-off pair and 24 generated E9 triplets'%len(paths))


if __name__=='__main__':main()
