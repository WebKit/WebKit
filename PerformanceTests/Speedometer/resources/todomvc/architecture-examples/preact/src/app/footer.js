import { h, Component } from 'preact';
import { pluralize } from './util';

export default class TodoFooter extends Component {
    render({ nowShowing, count, completedCount, onClearCompleted }) {
        return (
            <footer class="footer">
                <span class="todo-count">
                    <strong>{count}</strong> {pluralize(count, 'item')} left
                </span>
                <ul class="filters">
                    <li>
                        <a href="#/" class={nowShowing=='all' && 'selected'}>All</a>
                    </li>
                    {' '}
                    <li>
                        <a href="#/active" class={nowShowing=='active' && 'selected'}>Active</a>
                    </li>
                    {' '}
                    <li>
                        <a href="#/completed" class={nowShowing=='completed' && 'selected'}>Completed</a>
                    </li>
                </ul>
                { completedCount > 0 && (
                    <button class="clear-completed" onClick={onClearCompleted}>
                        Clear completed
                    </button>
                ) }
            </footer>
        );
    }
}
