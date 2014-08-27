'use strict';

(function() {
    var app = angular.module('app', ['ui.router', 'ngResource']);

    app.config(function($stateProvider, $urlRouterProvider) {
        $urlRouterProvider.otherwise('/');
        $stateProvider
            .state('tune', {
                url: '/',
                templateUrl: 'templates/tune.html',
                controller: 'tuneController'
            })
            .state('register', {
                url: '/register',
                templateUrl: 'templates/register.html',
            });
    });

    app.factory('volumeService', function($http) {
        var URL = 'api/volume';

        return {
            status: function(status) {
                if (arguments.length == 0)
                    return $http.get(URL);
                return $http.put(URL, status);
            }
        };
    });

    app.factory('tuneService', function($http) {
        return {
            set: function(station) {
                return $http.put('api/tune', station);
            }
        };
    });

    app.factory('stationService', function($resource) {
        return $resource('api/station/:id', { id: '@id' });
    });

    app.controller('mainController', function($scope, volumeService) {
        $scope.controllerName = 'mainController';
        volumeService.status()
            .success(function(data) {
                $scope.volumeStatus = data;
            })
            .error(function(reson) {
                $scope.volumeStatus = reson;
            });
        $scope.$watch('volumeStatus', function(newValue, oldValue) {
            if (newValue !== undefined && oldValue !== undefined)
                volumeService.status(newValue);
        }, true);
    });

    app.controller('tuneController', function($scope, stationService, tuneService) {
        $scope.controllerName = 'tuneController';
        $scope.stations = stationService.query();
        $scope.tune = function(index) {
            tuneService.set($scope.stations[index]);
        };
    });
})();
