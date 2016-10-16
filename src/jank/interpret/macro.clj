(ns jank.interpret.macro
  (:require [jank.parse.fabricate :as fabricate]
            [jank.type.scope.type-declaration :as type-declaration]
            [jank.type.expression :as expression]
            [jank.interpret.scope.value :as value])
  (:use jank.assert
        jank.debug.log))

; TODO: Check first, then check scope
(def prelude {{:name "print!"
                :argument-types [(fabricate/type "string")]} pprint
               {:name "print!"
                :argument-types [(fabricate/type "integer")]} pprint
               {:name "print!"
                :argument-types [(fabricate/type "real")]} pprint
               {:name "print!"
                :argument-types [(fabricate/type "boolean")]} pprint})

(defmulti evaluate-item
  (fn [item scope]
    (:kind item)))

(defn evaluate
  [body scope]
  ;(pprint (clean-scope body))
  (reduce #(let [item (evaluate-item %2 (:scope %1))]
             (assoc %1
                    :cells (conj (:cells %1) item)
                    :scope (:scope item)))
          {:cells []
           :scope scope}
          body))

(defmethod evaluate-item :macro-call
  [item scope]
  ; TODO: If external, the function must be in prelude
  (let [argument-pairs (map #(vector (:name %1)
                                     (evaluate-item %2 scope))
                            ; TODO: Add value for AST
                            (rest (get-in item [:definition :arguments :values]))
                            (get-in item [:definition :arguments :actual-arguments]))
        updated-item (update-in item
                                [:definition :scope]
                                (fn [inner-scope]
                                  (reduce (fn [acc [name value]]
                                            (value/add-to-scope name value acc))
                                          inner-scope
                                          argument-pairs)))
        body (evaluate (get-in updated-item [:definition :body])
                       (get-in updated-item [:definition :scope]))]
    (-> (assoc-in item [:definition :body] (:cells body))
        (assoc :scope (:scope body)))))

(defmethod evaluate-item :function-call
  [item scope]
  (let [signature {:name (-> item :name :name)
                   :argument-types (map (comp type-declaration/strip
                                              #(expression/realize-type % (:scope item)))
                                        (:arguments item))}
        arguments (map #(evaluate-item % scope) (:arguments item))
        _ (pprint (clean-scope arguments))
        func (if-let [f (get prelude signature)]
               f
               (not-yet-implemented interpret-assert "non-prelude functions"))]
    (interpret-assert func (str "unknown function " signature))
    (apply func (map :interpreted-value arguments))
    (assoc item :scope scope)))

; TODO: Assoc values onto each of these items
(defmethod evaluate-item :string
  [item scope]
  (assoc item :scope scope))

(defmethod evaluate-item :integer
  [item scope]
  (assoc item :scope scope))

(defmethod evaluate-item :real
  [item scope]
  (assoc item :scope scope))

(defmethod evaluate-item :boolean
  [item scope]
  (assoc item :scope scope))

(defmethod evaluate-item :identifier
  [item scope]
  ; TODO: If value hasn't been evaluated (may be a def), do so
  (assoc item
         :interpreted-value (:value (value/lookup (:name item) scope))
         :scope scope))

(defmethod evaluate-item :return
  [item scope]
  ; TODO
  (assoc item :scope scope))

(defmethod evaluate-item :default
  [item scope]
  (interpret-assert false (str "no supported evaluation for '" item "'")))