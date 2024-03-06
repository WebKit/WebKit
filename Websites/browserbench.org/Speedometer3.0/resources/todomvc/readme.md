# ![TodoMVC](./logo.png)

## Description

A to-do application allows a user to keep track of tasks that need to get done. The user can enter a new task, update an existing task, mark a task as completed, or delete a task. In addition to the basic CRUD operations, the TodoMVC app has some added functionality. The user can select or deselect the currently visible tasks and has the option to remove all completed tasks entirely. In addition, filters are available to change the view to “all”, “active” or “completed” tasks. A status text displays the number of active tasks to complete.

The to-do applications have been ported from [TodoMVC](https://todomvc.com/) and have been adapted to serve as workload applications for Speedometer 3.

## Screenshot

![screenshot](./screenshot.png)

## What are we testing

-   Stress-test DOM manipulations of a repeated action.
-   Impact of JavaScript version releases and their language features.
-   Tools (bundlers & transpilers) for build optimizations.
-   Libraries & frameworks for render strategies and architectural patterns

## How are we testing

The test that repeats a set number of times allows us to stress-test DOM manipulation by adding, updating and removing an element. This simulates a real-world user flow that is common for a web application. While the test is running, the benchmark tool captures performance metrics, to be able to compare the different workloads (apps).

The test consist of the following steps:

-   Wait for the “add task” input field to be present in the DOM
-   Add 100 tasks
-   Check off all tasks
-   Delete all tasks
