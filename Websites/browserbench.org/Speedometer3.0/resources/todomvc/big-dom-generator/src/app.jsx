import { ShowMore } from "./components/show-more.jsx";
import { Ribbon } from "./components/ribbon.jsx";
import { TopBar } from "./components/top-bar.jsx";
import { TreeArea } from "./components/tree-area.jsx";
import "./app.css";

const TodoArea = () => {
    return <div className="todo-area" />;
};

export const App = () => {
    return (
        <div className="main-ui" dir="ltr">
            <ShowMore />
            <Ribbon />
            <TopBar />
            <TreeArea />
            <TodoArea />
        </div>
    );
};
