import {useState} from 'preact/hooks';
import {Glyph} from './Glyph';
import {useKeys} from '../hooks/useKeys';
import {useOkPress} from '../hooks/useOkPress';
import {launchApp, appExists} from '../lib/launch';
import {AIRPLAY_ID} from '../config/constants';
interface Props {onClose:()=>void; receiverName?:string;}
export function AirPlayPanel({onClose,receiverName="Lumen TV"}:Props) {
    const [starting,setStarting]=useState(false);
    const [error,setError]=useState('');
    const start=()=>{
        if(starting)return;
        setStarting(true);setError('');
        appExists(AIRPLAY_ID).then(exists=>{
            if(!exists)throw new Error('De AirPlay-ontvanger is nog niet geïnstalleerd.');
            return launchApp(AIRPLAY_ID);
        }).then(onClose).catch(e=>{setError(String(e.message||e));setStarting(false);});
    };
    useKeys(key=>{if(key==='back'){onClose();return true;}return key==='enter';});
    useOkPress({onPress:start,onLongPress:()=>undefined},!starting);
    return <>
        <div class="panel-backdrop" onClick={onClose}/>
        <div class="panel panel--airplay">
            <div class="airplay-symbol"><Glyph name="airplay"/></div>
            <div class="panel__title">AirPlay</div>
            <p>Deel je scherm of speel een YouTube-video af op deze tv.</p>
            <div class="airplay-steps">
                <div><span>1</span>Start AirPlay hieronder.</div>
                <div><span>2</span>Kies <strong>{receiverName}</strong> op je iPhone, iPad of Mac.</div>
                <div><span>3</span>Vul de code in als je apparaat daarom vraagt.</div>
            </div>
            <p class="airplay-note">Na de sessie kom je terug op het beginscherm. Druk op Terug om AirPlay te verlaten.</p>
            {error&&<div class="airplay-error">{error}</div>}
            <button class="airplay-start" onClick={start} disabled={starting}>{starting?'AirPlay starten…':'AirPlay starten'}</button>
        </div>
    </>;
}
