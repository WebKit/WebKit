import SearchIcon from "./../assets/Smock_Search_18_N.svg";
import CrossIcon from "./../assets/CrossSize100.svg";

export const SearchArea = () => {
    return (
        <form className="spectrum-Search">
            <div className="spectrum-Textfield spectrum-Search-textfield">
                <SearchIcon className="spectrum-Icon spectrum-Icon--sizeM spectrum-Textfield-icon spectrum-Search-icon" focusable="false" aria-hidden="true" id="spectrum-icon-18-Magnify" />
                <input type="search" placeholder="Search" name="search" defaultValue="" className="spectrum-Textfield-input spectrum-Search-input" autoComplete="off" />
            </div>
            <button type="reset" className="spectrum-ClearButton spectrum-ClearButton--sizeM spectrum-Search-clearButton">
                <div className="spectrum-ClearButton-fill">
                    <CrossIcon className="spectrum-Icon spectrum-ClearButton-icon spectrum-UIIcon-Cross100" focusable="false" />
                </div>
            </button>
        </form>
    );
};
