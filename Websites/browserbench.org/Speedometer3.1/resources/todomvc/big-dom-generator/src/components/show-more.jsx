import { ActionButton } from "./action-group";
import { OptionsPopOver } from "./popovers/popover";

import MoreIcon from "./../assets/Smock_MoreCircle_18_N.svg";

export const ShowMore = () => {
    return (
        <div className="show-more">
            <ActionButton Icon={MoreIcon} quiet />
            <OptionsPopOver numOptions={10} />
        </div>
    );
};
