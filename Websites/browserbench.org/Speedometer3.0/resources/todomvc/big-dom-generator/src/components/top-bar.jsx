import { SearchArea } from "./search-area";
import { ActionButton, ActionGroup, ActionItem } from "./action-group";
import { OptionsPopOver } from "./popovers/popover";
import { NotificationsPopOver } from "./popovers/notifications";
import classNames from "classnames";

import ProfileIcon from "./../assets/Smock_RealTimeCustomerProfile_18_N.svg";
import SettingsIcon from "./../assets/Smock_Settings_18_N.svg";
import BellIcon from "./../assets/Smock_Bell_18_N.svg";
import HelpIcon from "./../assets/Smock_Help_18_N.svg";
import SpeedometerLogo from "./../assets/logo.png";

const ContextualHelp = () => {
    return (
        <>
            <ActionButton Icon={HelpIcon} quiet={false} className="spectrum-ActionGroup-item" />
            <div role="presentation" className={classNames("spectrum-Popover", "spectrum-Popover--sizeM", "spectrum-Popover--bottom-start", "spectrum-ContextualHelp-popover")}>
                <div className="context-help-popover-body">
                    <h2 className="spectrum-ContextualHelp-heading">Todo help</h2>
                    <p className="spectrum-ContextualHelp-body">Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.</p>
                </div>
            </div>
        </>
    );
};

const ProfileCardPopOver = () => {
    return (
        <div className="spectrum-Popover spectrum-Popover--bottom-right profile-card-popover" role="dialog">
            <div className="spectrum-Card spectrum-Card--sizeS" tabIndex="0" role="figure">
                <div className="spectrum-Card-coverPhoto"></div>
                <div className="spectrum-Card-body">
                    <div className="spectrum-Card-header">
                        <div className="spectrum-Card-title spectrum-Heading spectrum-Heading--sizeXS">John Doe</div>
                    </div>
                    <div className="spectrum-Card-content">
                        <div className="spectrum-Card-subtitle spectrum-Detail spectrum-Detail--sizeXS">
                            <p>jdoe</p>
                        </div>
                    </div>
                </div>
                <a className="spectrum-Card-footer">Sign in with a different account</a>
            </div>
        </div>
    );
};

export const TopBar = () => {
    const numSettings = 5;
    return (
        <div className="top-bar">
            <img src={SpeedometerLogo} alt="Speedometer Logo for TODO App" height={40} />
            <div className={"search-area"}>
                <SearchArea />
            </div>
            <div className="top-bar-buttons">
                <ActionGroup>
                    <ContextualHelp />
                    <ActionItem className="notifications-group">
                        <ActionButton className="spectrum-ActionGroup-item" Icon={BellIcon} quiet={false} />
                        <NotificationsPopOver />
                    </ActionItem>

                    <ActionItem>
                        <ActionButton className="spectrum-ActionGroup-item" Icon={SettingsIcon} quiet={false} />
                        <OptionsPopOver numOptions={numSettings} startRight={true} />
                    </ActionItem>
                    <ActionItem>
                        <ActionButton className="spectrum-ActionGroup-item" Icon={ProfileIcon} quiet={false} />
                        <ProfileCardPopOver />
                    </ActionItem>
                </ActionGroup>
            </div>
        </div>
    );
};
