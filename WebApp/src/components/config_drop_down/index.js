import { useSignal } from '@preact/signals';
import Select from 'preact-material-components/Select';
import { useEffect } from 'preact/hooks';
import useInterval from '../../utils/use_interval';

// SOURCE:
// https://material.preactjs.com/component/select/
const ConfigDropDown = ({
    patternIdx,
    initial,
    structureRef,
    update
}) => {
    
    const configs = [
        ["None"],
        ["Default"],
        ["Default"],
        ["Default", "Blur", "Shift"],
        ["Default", "Octaves", "Shift", "Compression"],
        ["Default", "Shift"],
        ["Hue", "Formants", "Moving"],
        ["Default", "Sections", "Talk"],
        ["Basic", "Advanced", "Formant"],
        ["Default"],
        ["Default"],
        ["Frequency", "Volume"],
        ["Default"],
        ["Volume", "Frequency"],
        ["Default"],
        ["Default"],
        ["Default"],
        ["Default"],
        ["Default"],
        ["Default"],
        ["Default"]
    ]

    const current = useSignal(initial);

    /**
     * @brief Checks if the set config is greater than the number
     * of configs. Sets the config to 0 if this happens.
     */
    const checkBounds = () => {
        if(current.value > configs[patternIdx].length){
            current.value = 0;
            update(structureRef, current.value);
        }
    }

    useInterval(() => {checkBounds()}, 100)

    return (
        <div>
            <Select
                selectedIndex={current.value}
                onChange={(e)=>{
                    current.value = e.target.selectedIndex;
                    update(structureRef, current);
            }}>
                {(patternIdx <= configs.length && patternIdx >= 0) ?
                    
                    configs[patternIdx].map((optionName) => {
                        return <Select.Item>{optionName}</Select.Item>;
                    })
                    
                    :
                    configs[0].map((optionName) => {
                        return <Select.Item>{optionName}</Select.Item>;
                    })

                }
            </Select>
        </div>
    );
}

export default ConfigDropDown;